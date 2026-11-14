// BoatControl — NVS-хранение runtime-параметров (SPEC п.11).
// Формат и защита — как у калибровки хода (calibration.cpp): заголовок
// magic+version, тело, FNV-1a CRC, подтверждение обратным чтением.

#include "params.h"
#include <cstddef>
#include <Preferences.h>

struct ParamsBlob {
    uint16_t magic;
    uint16_t version;
    Params   data;
    uint32_t crc;
};

static constexpr uint16_t PARAMS_MAGIC = 0xBC42;    // 'BC' + 'B'(yparams)
static constexpr uint16_t PARAMS_NVS_VERSION = 1;   // версия формата Params
static const char* PARAMS_KEY = "prm";

static uint32_t params_crc(const ParamsBlob* b) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(b);
    size_t n = offsetof(ParamsBlob, crc);
    uint32_t h = 2166136261UL;
    for (size_t i = 0; i < n; i++) {
        h ^= p[i];
        h *= 16777619UL;
    }
    return h;
}

bool params_load(Params* p) {
    Preferences prefs;
    if (!prefs.begin(CAL_NVS_NAMESPACE, true /*readonly*/)) return false;

    size_t got = prefs.getBytes(PARAMS_KEY, nullptr, 0);
    if (got != sizeof(ParamsBlob)) {
        prefs.end();
        return false;
    }

    ParamsBlob blob;
    got = prefs.getBytes(PARAMS_KEY, &blob, sizeof(blob));
    prefs.end();

    if (got != sizeof(blob)) return false;
    if (blob.magic != PARAMS_MAGIC) return false;
    if (blob.version != PARAMS_NVS_VERSION) return false;
    if (blob.crc != params_crc(&blob)) return false;
    if (!params_validate(blob.data)) return false;

    *p = blob.data;
    return true;
}

bool params_store(const Params& p) {
    if (!params_validate(p)) return false;

    ParamsBlob blob;
    blob.magic = PARAMS_MAGIC;
    blob.version = PARAMS_NVS_VERSION;
    blob.data = p;
    blob.crc = params_crc(&blob);

    Preferences prefs;
    if (!prefs.begin(CAL_NVS_NAMESPACE, false)) return false;
    size_t written = prefs.putBytes(PARAMS_KEY, &blob, sizeof(blob));
    prefs.end();
    if (written != sizeof(blob)) return false;

    // Как и с калибровкой: ложный успех NVS дороже отказа записи.
    Params check;
    return params_load(&check) &&
           memcmp(&check, &p, sizeof(Params)) == 0;
}

bool params_erase() {
    Preferences prefs;
    if (!prefs.begin(CAL_NVS_NAMESPACE, false)) return false;
    prefs.remove(PARAMS_KEY);
    prefs.end();
    return true;
}
