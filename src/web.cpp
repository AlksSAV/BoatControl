// BoatControl — сервисный веб (SPEC п.11). Реализация; интерфейс и политика
// окна — в web.h. Всё исполняется в контексте loop(): очередей задач нет,
// блокировок нет, один WebServer на порт 80.

#include "web.h"

#if WEB_ENABLE

#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "calibration.h"

// --- Состояние сервиса (живёт только здесь) ---
static WebServer*   g_srv = nullptr;
static bool         g_ap_up = false;       // AP физически поднята
static bool         g_ap_used = false;     // хоть одна станция ассоциировалась
static uint32_t     t_ap_start_ms = 0;
static uint32_t     t_ap_seen_ms = 0;      // последний такт, когда была станция
static uint32_t     t_poll_ms = 0;         // периодических задач немного

static WebCmd       g_queue[WEB_CMD_QUEUE_LEN];
static uint8_t      g_q_head = 0;
static uint8_t      g_q_tail = 0;

// Последний снимок телеметрии: loop() обновляет его перед web_update(),
// обработчик /api/status читает эту копию.
static WebStatus    g_snapshot;

static void queue_cmd(WebCmd c) {
    uint8_t next = (uint8_t)((g_q_head + 1) % WEB_CMD_QUEUE_LEN);
    if (next == g_q_tail) return;  // очередь полна — команда теряется (виден отказ в логе)
    g_queue[g_q_head] = c;
    g_q_head = next;
}

bool web_take_cmd(WebCmd* out) {
    if (g_q_tail == g_q_head) return false;
    *out = g_queue[g_q_tail];
    g_q_tail = (uint8_t)((g_q_tail + 1) % WEB_CMD_QUEUE_LEN);
    return true;
}

// --- Гашение AP. После неё веб не поднимается до перезапитки (SPEC п.11). ---
static void ap_shutdown() {
    if (!g_ap_up) return;
    g_ap_up = false;
    if (g_srv) {
        g_srv->close();
        delete g_srv;
        g_srv = nullptr;
    }
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
}

void web_close() { ap_shutdown(); }

bool web_active() { return g_ap_up; }

// --- Разбор тела POST /api/params: плоский JSON, только известные ключи. ---
// Все значения сначала применяются к копии, валидируются целиком и только
// потом коммитятся в g_params + NVS. Рабочий набор не может стать частичным.
static void handle_params_post(const String& body) {
    DynamicJsonDocument doc(1024);
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        g_srv->send(400, F("application/json"), F("{\"ok\":false,\"err\":\"bad json\"}"));
        return;
    }

    Params p;  // копия текущего рабочего набора
    p = g_params;
    bool any = false;
    bool bad = false;

    auto check = [&](bool ok) { if (ok) any = true; else bad = true; };
    auto u16 = [&](const char* k) { return (uint16_t)doc[k].as<long>(); };
    auto u8 = [&](const char* k) { return (uint8_t)doc[k].as<long>(); };

    if (!doc["thr_span"].isNull())        check(params_clamp_thr_span(p, u16("thr_span")));
    if (!doc["thr_deadband"].isNull())    check(params_clamp_thr_deadband(p, u16("thr_deadband")));
    if (!doc["thr_rate_up"].isNull())     check(params_clamp_thr_rate_up(p, u8("thr_rate_up")));
    if (!doc["thr_rate_down"].isNull())   check(params_clamp_thr_rate_down(p, u8("thr_rate_down")));
    if (!doc["thr_zero_ms"].isNull())     check(params_clamp_thr_zero_pass(p, u16("thr_zero_ms")));
    if (!doc["x_span"].isNull())          check(params_clamp_x_span(p, u16("x_span")));
    if (!doc["x_deadband"].isNull())      check(params_clamp_x_deadband(p, u16("x_deadband")));
    if (!doc["x_exponent"].isNull())      check(params_clamp_x_exponent(p, doc["x_exponent"].as<float>()));
    if (!doc["stb_slew_ms"].isNull())     check(params_clamp_stb_slew_full(p, u16("stb_slew_ms")));
    if (!doc["stb_min_us"].isNull())      check(params_clamp_stb_pulse_min(p, u16("stb_min_us")));
    if (!doc["stb_max_us"].isNull())      check(params_clamp_stb_pulse_max(p, u16("stb_max_us")));
    if (!doc["cruise_min_pct"].isNull())  check(params_clamp_cruise_min_pct(p, u8("cruise_min_pct")));
    if (!doc["cruise_step_pct"].isNull()) check(params_clamp_cruise_step_pct(p, u8("cruise_step_pct")));
    if (!doc["cruise_edge_pct"].isNull()) check(params_clamp_cruise_edge_pct(p, u8("cruise_edge_pct")));
    if (!doc["cruise_ramp_ms"].isNull())  check(params_clamp_cruise_ramp_ms(p, u16("cruise_ramp_ms")));

    if (!any || bad || !params_validate(p)) {
        g_srv->send(400, F("application/json"), F("{\"ok\":false,\"err\":\"out of range\"}"));
        return;
    }

    if (!params_store(p)) {
        g_srv->send(500, F("application/json"), F("{\"ok\":false,\"err\":\"nvs write\"}"));
        return;
    }
    g_params = p;  // commit: loop на следующем такте работает новыми значениями
    g_srv->send(200, F("application/json"), F("{\"ok\":true}"));
}

// --- Ответ /api/status: телеметрия + параметры + границы + состояние окна. ---
static void handle_status(const WebStatus& st) {
    DynamicJsonDocument doc(2048);
    JsonObject o = doc.to<JsonObject>();
    o["fw"] = FIRMWARE_VERSION;
    o["state"] = st.state;
    o["esc"] = st.esc_us;
    o["stb"] = st.servo_us;
    o["ax"] = st.ax_raw;
    o["ay"] = st.ay_raw;
    o["xoff"] = st.x_off;
    o["yoff"] = st.y_off;
    o["cx"] = st.center_x;
    o["cy"] = st.center_y;
    o["sx"] = st.span_x;
    o["sy"] = st.span_y;
    o["crs"] = st.cruise_us;
    o["cal_pending"] = st.cal_pending;

    JsonObject p = o["prm"].to<JsonObject>();
    p["thr_span"] = g_params.thr_span_adc;
    p["thr_deadband"] = g_params.thr_deadband_adc;
    p["thr_rate_up"] = g_params.thr_rate_up_pct;
    p["thr_rate_down"] = g_params.thr_rate_down_pct;
    p["thr_zero_ms"] = g_params.thr_zero_pass_ms;
    p["x_span"] = g_params.x_span_adc;
    p["x_deadband"] = g_params.x_deadband_adc;
    p["x_exponent"] = g_params.x_exponent;
    p["stb_slew_ms"] = g_params.stb_slew_full_ms;
    p["stb_min_us"] = g_params.stb_pulse_min;
    p["stb_max_us"] = g_params.stb_pulse_max;
    p["cruise_min_pct"] = g_params.cruise_min_pct;
    p["cruise_step_pct"] = g_params.cruise_step_pct;
    p["cruise_edge_pct"] = g_params.cruise_edge_pct;
    p["cruise_ramp_ms"] = g_params.cruise_ramp_ms;

    JsonObject b = o["bnd"].to<JsonObject>();
    b["thr_span"] = THR_SPAN_ADC_MIN;      b["thr_span_x"] = THR_SPAN_ADC_MAX;
    b["thr_deadband"] = THR_DB_ADC_MIN;    b["thr_deadband_x"] = THR_DB_ADC_MAX;
    b["thr_rate_up"] = THR_RATE_PCT_MIN;   b["thr_rate_up_x"] = THR_RATE_PCT_MAX;
    b["thr_rate_down"] = THR_RATE_PCT_MIN; b["thr_rate_down_x"] = THR_RATE_PCT_MAX;
    b["thr_zero_ms_x"] = THR_ZERO_MS_MAX;
    b["x_span"] = X_SPAN_ADC_MIN;          b["x_span_x"] = X_SPAN_ADC_MAX;
    b["x_deadband"] = X_DB_ADC_MIN;        b["x_deadband_x"] = X_DB_ADC_MAX;
    b["x_exponent"] = (int)(X_EXPONENT_MIN * 100);      b["x_exponent_x"] = (int)(X_EXPONENT_MAX * 100);
    b["stb_slew_ms"] = STB_SLEW_MS_MIN;    b["stb_slew_ms_x"] = STB_SLEW_MS_MAX;
    b["stb_min_us"] = STB_PULSE_MIN_MIN;
    b["stb_max_us_x"] = STB_PULSE_MAX_MAX;
    b["cruise_min_pct"] = CRUISE_MIN_PCT_MIN; b["cruise_min_pct_x"] = CRUISE_MIN_PCT_MAX;
    b["cruise_step_pct"] = CRUISE_STEP_PCT_MIN; b["cruise_step_pct_x"] = CRUISE_STEP_PCT_MAX;
    b["cruise_edge_pct"] = CRUISE_EDGE_PCT_MIN; b["cruise_edge_pct_x"] = CRUISE_EDGE_PCT_MAX;
    b["cruise_ramp_ms"] = CRUISE_RAMP_MS_MIN;  b["cruise_ramp_ms_x"] = CRUISE_RAMP_MS_MAX;

    uint32_t remain_ms = 0;
    if (g_ap_used) {
        remain_ms = WEB_AP_IDLE_CLOSE_MS;  // клиент подключён — окно не тикает вниз
    } else if (t_ap_start_ms + WEB_AP_TIMEOUT_MS > millis()) {
        remain_ms = WEB_AP_TIMEOUT_MS - (millis() - t_ap_start_ms);
    }
    JsonObject w = o["web"].to<JsonObject>();
    w["active"] = g_ap_up;
    w["remain_s"] = (uint16_t)(remain_ms / 1000UL);

    String out;
    serializeJson(doc, out);
    g_srv->send(200, F("application/json"), out);
}

// Страница: одна форма, опрос /api/status каждые WEB_POLL_MS.
static const char INDEX_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="ru"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>BoatControl сервис</title>
<style>
:root{color-scheme:dark;--bg:#12161c;--card:#1b212b;--tx:#dde4ee;--mut:#8b98ab;
--ok:#4caf7d;--warn:#e0a63c;--err:#e05c5c;--ac:#4d8fd1}
*{box-sizing:border-box}
body{margin:0;padding:12px;background:var(--bg);color:var(--tx);
font:14px/1.45 system-ui,sans-serif;max-width:860px;margin-inline:auto}
h1{font-size:17px;margin:2px 0 10px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(190px,1fr));gap:8px}
.card{background:var(--card);border-radius:8px;padding:10px 12px}
.k{color:var(--mut);font-size:12px}.v{font-size:17px;font-variant-numeric:tabular-nums}
#state{font-weight:700}
table{width:100%;border-collapse:collapse}
td{padding:3px 4px;vertical-align:middle}
td:first-child{color:var(--mut);white-space:nowrap;padding-right:8px}
input[type=number]{width:100%;background:#0e1218;color:var(--tx);border:1px solid #2b3542;
border-radius:5px;padding:5px 7px;font:inherit}
button{background:var(--ac);color:#fff;border:0;border-radius:6px;padding:9px 14px;
font:inherit;font-weight:600;cursor:pointer;margin:3px 4px 3px 0}
button.sec{background:#37414f}
button:disabled{opacity:.45;cursor:default}
#msg{min-height:20px;margin-top:6px;font-weight:600}
.msg-ok{color:var(--ok)}.msg-err{color:var(--err)}.msg-warn{color:var(--warn)}
#timer{color:var(--warn);font-weight:700}
.sec-h{font-size:13px;color:var(--mut);margin:14px 0 6px;text-transform:uppercase;letter-spacing:.4px}
</style></head><body>
<h1>BoatControl <span id="fw"></span> &nbsp;<span id="timer"></span></h1>
<div class="grid" id="tele"></div>
<div class="sec-h">Параметры (сохранение — в flash, действует сразу)</div>
<div class="card"><table id="prm"></table>
<div><button id="save">Сохранить</button>
<button id="erase" class="sec">Сброс к config.h</button></div></div>
<div class="sec-h">Сервис</div>
<div class="card"><div>
<button id="cal">Калибровка хода (15&nbsp;с)</button>
<button id="close" class="sec">Закрыть веб</button></div>
<div>Калибровка доступна в READY: удерживать не нужно, стик переводить вручную.
Веб закрывается при начале движения или по таймауту окна.</div></div>
<div id="msg"></div>
<script>
const F=[["thr_span","Газ: полный ход, АЦП",1],
["thr_deadband","Газ: мёртвая зона, АЦП",1],
["thr_rate_up","Газ: набор, %/100мс",1],
["thr_rate_down","Газ: сброс, %/100мс",1],
["thr_zero_ms","Газ: пауза смены знака, мс",1],
["x_span","Руль: полный ход, АЦП",1],
["x_deadband","Руль: мёртвая зона, АЦП",1],
["x_exponent","Руль: экспонента",0.01],
["stb_slew_ms","Руль: ход упор-упор, мс",1],
["stb_min_us","Руль: упор левый, мкс",1],
["stb_max_us","Руль: упор правый, мкс",1],
["cruise_min_pct","Круиз: мин. газ, %",1],
["cruise_step_pct","Круиз: шаг, %",1],
["cruise_edge_pct","Круиз: порог упора, %",1],
["cruise_ramp_ms","Круиз: возврат, мс",1]];
const T=[["Состояние","state",0],["ESC","esc"," мкс"],["Серва","stb"," мкс"],
["Стик X (сырой)","ax",0],["Стик Y (сырой)","ay",0],["Стик X (ход)","xoff",0],
["Стик Y (ход)","yoff",0],["Центр X","cx",0],["Центр Y","cy",0],
["Полуход X","sx",0],["Полуход Y","sy",0],["Круиз","crs"," мкс"]];
const tele=document.getElementById('tele'),prm=document.getElementById('prm'),
msg=document.getElementById('msg'),timer=document.getElementById('timer');
let inputs={};
F.forEach(([k,label,st])=>{const tr=document.createElement('tr');
tr.innerHTML=`<td>${label}</td>`;const td=document.createElement('td');
const i=document.createElement('input');i.type='number';i.step=st;i.id=k;
td.appendChild(i);tr.appendChild(td);prm.appendChild(tr);inputs[k]=i;});
function card(k,l,v){return `<div class="card"><div class="k">${l}</div><div class="v" id="t_${k}">${v}</div></div>`}
tele.innerHTML=T.map(([l,k])=>card(k,l,'—')).join('');
function setMsg(t,c){msg.textContent=t;msg.className=c||''}
function stColor(s){return s==='READY'?'var(--ok)':s==='FAULT'?'var(--err)':
(s==='DRIVE'||s==='CRUISE')?'var(--warn)':'var(--tx)'}
async function poll(){try{
const r=await fetch('/api/status');if(!r.ok)throw 0;const d=await r.json();
document.getElementById('fw').textContent=d.fw;
T.forEach(([l,k])=>{const e=document.getElementById('t_'+k);
e.textContent=(k==='state')?d[k]:d[k]+(k==='esc'||k==='stb'||k==='crs'?' мкс':'');
if(k==='state')e.style.color=stColor(d[k])});
for(const[k]of F){const i=inputs[k];if(document.activeElement!==i)i.value=d.prm[k];
if(d.bnd[k]!==undefined)i.min=d.bnd[k];if(d.bnd[k+'_x']!==undefined)i.max=d.bnd[k+'_x'];}
timer.textContent=d.web.active&&!d.web.remain_s?'окно закрыто, перезапустите':'окно: '+d.web.remain_s+' с';
if(!d.web.active){timer.className='msg-err';
document.querySelectorAll('button').forEach(b=>b.disabled=true);}
}catch(e){timer.textContent='нет связи — веб закрыт?';timer.className='msg-err'}}
setInterval(poll,500);poll();
async function post(url,body){try{const r=await fetch(url,{method:'POST',
headers:{'Content-Type':'application/json'},body:body?JSON.stringify(body):''});
const d=await r.json();return{ok:r.ok&&d.ok,err:d.err||r.status}}
catch(e){return{ok:false,err:'нет связи'}}}
document.getElementById('save').onclick=async()=>{
const b={};for(const[k]of F){const v=inputs[k].value;if(v!=='')b[k]=parseFloat(v);}
const r=await post('/api/params',b);
setMsg(r.ok?'сохранено в flash':('отклонено: '+(r.err||'')),(r.ok?'msg-ok':'msg-err'));poll()};
document.getElementById('erase').onclick=async()=>{
if(!confirm('Сбросить все параметры к константам config.h?'))return;
const r=await post('/api/erase');
setMsg(r.ok?'сброшено, значения по умолчанию':('ошибка: '+r.err),(r.ok?'msg-ok':'msg-err'));poll()};
document.getElementById('cal').onclick=async()=>{
if(!confirm('Начать калибровку хода? Выходы в нейтрали, 15 с переводить стик во все упоры.'))return;
const r=await post('/api/calibrate');
setMsg(r.ok?'калибровка запущена (если состояние READY)':('отказ: '+r.err),
(r.ok?'msg-warn':'msg-err'))};
document.getElementById('close').onclick=async()=>{
if(!confirm('Закрыть веб до следующей перезапитки?'))return;
await post('/api/close');setMsg('веб закрыт','msg-warn')};
</script></body></html>)rawliteral";

void web_init() {
    if (g_ap_up) return;

    WiFi.mode(WIFI_AP);
    WiFi.softAP(WEB_AP_SSID, WEB_AP_PASS);
    g_ap_up = true;
    t_ap_start_ms = millis();
    t_ap_seen_ms = t_ap_start_ms;

    g_srv = new WebServer(80);

    // handle_status нужен свежим снимком каждый запрос — храним указатель
    // на статический буфер снимка, который loop() обновляет перед web_update().
    g_srv->on("/", HTTP_GET, []() {
        g_srv->send_P(200, F("text/html"), INDEX_HTML);
    });
    g_srv->on("/api/status", HTTP_GET, []() {
        handle_status(g_snapshot);
    });
    g_srv->on("/api/params", HTTP_POST, []() {
        handle_params_post(g_srv->arg("plain"));
    });
    g_srv->on("/api/erase", HTTP_POST, []() {
        Params d;
        params_default(&d);
        if (params_store(d)) {
            g_params = d;
            g_srv->send(200, F("application/json"), F("{\"ok\":true}"));
        } else {
            g_srv->send(500, F("application/json"), F("{\"ok\":false,\"err\":\"nvs\"}"));
        }
    });
    g_srv->on("/api/calibrate", HTTP_POST, []() {
        queue_cmd(WebCmd::CALIBRATE);
        g_srv->send(200, F("application/json"), F("{\"ok\":true}"));
    });
    g_srv->on("/api/close", HTTP_POST, []() {
        g_close_queue = WebCmdClose::CLOSE;
        g_srv->send(200, F("application/json"), F("{\"ok\":true}"));
    });
    g_srv->onNotFound([]() { g_srv->send(404, F("text/plain"), "not found"); });

    g_srv->begin();
}

void web_update(const WebStatus& st, uint32_t now_ms) {
    if (!g_ap_up) return;

    g_snapshot = st;  // снимок для /api/status актуален на текущий такт

    // Политика окна (SPEC п.11): не подключились за WEB_AP_TIMEOUT_MS — гасим;
    // клиент пропал дольше WEB_AP_IDLE_CLOSE_MS — тоже гасим.
    if (WiFi.softAPgetStationNum() > 0) {
        g_ap_used = true;
        t_ap_seen_ms = now_ms;
    } else if (!g_ap_used && (now_ms - t_ap_start_ms) >= WEB_AP_TIMEOUT_MS) {
        ap_shutdown();
        return;
    } else if (g_ap_used && (now_ms - t_ap_seen_ms) >= WEB_AP_IDLE_CLOSE_MS) {
        ap_shutdown();
        return;
    }

    g_srv->handleClient();
}

#endif  // WEB_ENABLE
