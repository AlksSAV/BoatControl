// BoatControl — NVS-хранение калибровки (SPEC п.5.5).

#include "calibration.h"
#include <cstddef>
#include <Preferences.h>

// Прошло в NVS как BLOB: структура целиком, байтовый порядок не важен
// (читающая сторона та же прошивка). Заголовок и CRC ловят мусор и даунгрейд.
struct CalBlob {
    uint16_t magic;
    uint16_t version;
    CalData  data;
    uint32_t crc;
};

static constexpr uint16_t CAL_MAGIC = 0xBC41;  // 'BC' + 'A'(l)
static const char* CAL_KEY = "cal";

// FNV-1a по байтам тела (без crc-поля).
static uint32_t cal_crc(const CalBlob* b) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(b);
    size_t n = offsetof(CalBlob, crc);
    uint32_t h = 2166136261UL;
    for (size_t i = 0; i < n; i++) {
        h ^= p[i];
        h *= 16777619UL;
    }
    return h;
}

// Санитарная проверка значений (SPEC п.5.5): полуход в физически возможном диапазоне.
static bool cal_data_sane(const CalData* d) {
    if (d->span_x < CAL_MIN_SPAN_ADC || d->span_x > CAL_MAX_SPAN_ADC) return false;
    if (d->span_y < CAL_MIN_SPAN_ADC || d->span_y > CAL_MAX_SPAN_ADC) return false;
    return true;
}

bool cal_load(CalData* out) {
    Preferences prefs;
    if (!prefs.begin(CAL_NVS_NAMESPACE, true /*readonly*/)) return false;

    size_t got = prefs.getBytes(CAL_KEY, nullptr, 0);  // размер записи без чтения
    if (got != sizeof(CalBlob)) {
        prefs.end();
        return false;
    }

    CalBlob blob;
    got = prefs.getBytes(CAL_KEY, &blob, sizeof(blob));
    prefs.end();

    if (got != sizeof(blob)) return false;
    if (blob.magic != CAL_MAGIC) return false;
    if (blob.version != CAL_NVS_VERSION) return false;
    if (blob.crc != cal_crc(&blob)) return false;
    if (!cal_data_sane(&blob.data)) return false;

    *out = blob.data;
    return true;
}

bool cal_store(const CalData& data) {
    if (!cal_data_sane(&data)) return false;

    CalBlob blob;
    blob.magic = CAL_MAGIC;
    blob.version = CAL_NVS_VERSION;
    blob.data = data;
    blob.crc = cal_crc(&blob);

    Preferences prefs;
    if (!prefs.begin(CAL_NVS_NAMESPACE, false)) return false;
    size_t written = prefs.putBytes(CAL_KEY, &blob, sizeof(blob));
    prefs.end();
    if (written != sizeof(blob)) return false;

    // Ложный успех NVS дороже, чем повторная калибровка: читаем обратно.
    CalData check;
    return cal_load(&check) &&
           check.span_x == data.span_x && check.span_y == data.span_y;
}

bool cal_erase() {
    Preferences prefs;
    if (!prefs.begin(CAL_NVS_NAMESPACE, false)) return false;
    prefs.remove(CAL_KEY);
    prefs.end();
    return true;
}
