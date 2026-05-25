#include "signal_capture.h"
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_subghz.h>
#include <furi_hal_gpio.h>
#include <lib/subghz/devices/devices.h>
#include <string.h>

// Device names for internal and external CC1101
#ifndef SUBGHZ_DEVICE_CC1101_INT_NAME
#define SUBGHZ_DEVICE_CC1101_INT_NAME "cc1101_int"
#endif
#ifndef SUBGHZ_DEVICE_CC1101_EXT_NAME
#define SUBGHZ_DEVICE_CC1101_EXT_NAME "cc1101_ext"
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Frequency sweep table
// ─────────────────────────────────────────────────────────────────────────────

// CC1101 valid bands:  300-348 MHz | 387-464 MHz | 779-928 MHz
// Momentum extends to: 281-361 MHz | 378-481 MHz | 749-962 MHz
// NOTE: 149-152 MHz was removed — it is outside all valid bands and caused
//       a "SubGhz: Incorrect frequency during set" crash.
const uint32_t SWEEP_FREQUENCIES[] = {
    // --- Extended low band (Momentum: 281-361 MHz) ---
    290000000, 300000000,
    // --- 315 MHz band ---
    314900000, 315100000,
    // --- Extended mid (Momentum: up to ~361 MHz) ---
    345000000,
    // --- 433/434 MHz band ---
    433050000, 433420000, 433920000, 434420000, 434790000,
    434000000,
    // --- 400-405 MHz (pagers, telemetry) ---
    401000000, 403000000, 405000000,
    // --- Extended upper mid (Momentum: up to ~481 MHz) ---
    470000000,
    // --- Extended lower high band (Momentum: 749-779 MHz) ---
    750000000, 770000000,
    // --- 868 MHz (EU ISM) ---
    868000000, 868300000, 868500000, 869000000, 869525000,
    // --- 902-928 MHz (US ISM) ---
    902000000, 908000000, 915000000, 920000000, 928000000,
};

const uint16_t SWEEP_FREQ_COUNT = sizeof(SWEEP_FREQUENCIES) / sizeof(SWEEP_FREQUENCIES[0]);

// ─────────────────────────────────────────────────────────────────────────────
// Raw pulse buffer
// ─────────────────────────────────────────────────────────────────────────────

#define RAW_BUF_SIZE       512
#define CAPTURE_TIMEOUT_MS 400
#define MIN_PULSE_COUNT    16
#define SIGNAL_THRESHOLD_DBM -80.0f

typedef struct {
    uint32_t pulses[RAW_BUF_SIZE];
    uint16_t count;
    bool     capturing;
} RawBuffer;

static RawBuffer s_raw;

static void rx_callback(bool level, uint32_t duration, void* context) {
    UNUSED(level);
    UNUSED(context);
    if(!s_raw.capturing) return;
    if(s_raw.count < RAW_BUF_SIZE) {
        s_raw.pulses[s_raw.count++] = duration;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Context
// ─────────────────────────────────────────────────────────────────────────────

struct SignalCaptureCtx {
    ScanMode             mode;
    AntennaMode          antenna;      // what was requested
    bool                 antenna_ok;   // false if external device not found
    uint32_t             frequency;
    float                threshold;
    uint16_t             sweep_index;
    bool                 running;
    const SubGhzDevice*  device;       // active CC1101 device handle
};

// ─────────────────────────────────────────────────────────────────────────────
// RSSI history
// ─────────────────────────────────────────────────────────────────────────────

void rssi_history_push(RSSIHistory* h, float rssi) {
    h->values[h->head] = rssi;
    h->head = (h->head + 1) % RSSI_HISTORY_LEN;
    if(h->count < RSSI_HISTORY_LEN) h->count++;
}

float rssi_history_get(const RSSIHistory* h, uint8_t index) {
    if(index >= h->count) return -120.0f;
    uint8_t pos = (uint8_t)((h->head - h->count + index + RSSI_HISTORY_LEN) % RSSI_HISTORY_LEN);
    return h->values[pos];
}

// (Antenna selection is now handled via subghz_devices — see signal_capture_start)

// ─────────────────────────────────────────────────────────────────────────────
// Context lifecycle
// ─────────────────────────────────────────────────────────────────────────────

SignalCaptureCtx* signal_capture_alloc(void) {
    SignalCaptureCtx* ctx = malloc(sizeof(SignalCaptureCtx));
    furi_assert(ctx);
    ctx->mode         = ScanModeSubGHz;
    ctx->antenna      = AntennaInternal;
    ctx->antenna_ok   = true;
    ctx->frequency    = 0;
    ctx->threshold    = SIGNAL_THRESHOLD_DBM;
    ctx->sweep_index  = 0;
    ctx->running      = false;
    ctx->device       = NULL;
    memset(&s_raw, 0, sizeof(s_raw));
    return ctx;
}

void signal_capture_free(SignalCaptureCtx* ctx) {
    if(!ctx) return;
    signal_capture_stop(ctx);
    free(ctx);
}

void signal_capture_set_mode(SignalCaptureCtx* ctx, ScanMode mode) {
    ctx->mode = mode;
}

void signal_capture_set_antenna(SignalCaptureCtx* ctx, AntennaMode antenna) {
    // Device is selected in signal_capture_start based on this setting.
    // Changing it while running requires a restart.
    ctx->antenna = antenna;
}

void signal_capture_set_frequency(SignalCaptureCtx* ctx, uint32_t freq_hz) {
    ctx->frequency = freq_hz;
}

void signal_capture_set_threshold(SignalCaptureCtx* ctx, float threshold_dbm) {
    ctx->threshold = threshold_dbm;
}

// ─────────────────────────────────────────────────────────────────────────────
// Start / stop
// ─────────────────────────────────────────────────────────────────────────────

bool signal_capture_start(SignalCaptureCtx* ctx) {
    if(ctx->running) return true;

    // ── Device selection ──────────────────────────────────────────────────────
    // Use the SubGhz device abstraction so both the built-in CC1101 and an
    // external CC1101 (e.g. dev board) work transparently.
    // "cc1101_ext" is only available when the external module is connected.
    const char* dev_name = (ctx->antenna == AntennaExternal)
        ? SUBGHZ_DEVICE_CC1101_EXT_NAME
        : SUBGHZ_DEVICE_CC1101_INT_NAME;

    ctx->device = subghz_devices_get_by_name(dev_name);
    if(!ctx->device && ctx->antenna == AntennaExternal) {
        // External module not found — fall back silently to internal
        ctx->device     = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
        ctx->antenna_ok = false;
    } else {
        ctx->antenna_ok = (ctx->device != NULL);
    }
    // Hard guard — if we still have no device something is seriously wrong
    if(!ctx->device) {
        FURI_LOG_E("RFRosetta", "No CC1101 device found");
        return false;
    }

    // ── Radio init ────────────────────────────────────────────────────────────
    subghz_devices_begin(ctx->device);
    subghz_devices_reset(ctx->device);
    subghz_devices_idle(ctx->device);

    // ── Mode preset ───────────────────────────────────────────────────────────
    // OOK650  — standard OOK, 650 kHz BW — best for remotes, doorbells, sensors
    // FSK238  — narrow 2-FSK, 238 kHz dev — better sensitivity, less noise
    // FSK476  — wider  2-FSK, 476 kHz dev — catches more FSK signal types
    FuriHalSubGhzPreset preset;
    switch(ctx->mode) {
        case ScanModeRFNarrow: preset = FuriHalSubGhzPreset2FSKDev238Async; break;
        case ScanModeRFWide:   preset = FuriHalSubGhzPreset2FSKDev476Async; break;
        default:               preset = FuriHalSubGhzPresetOok650Async;     break;
    }
    subghz_devices_load_preset(ctx->device, preset, NULL);

    uint32_t freq = ctx->frequency > 0 ? ctx->frequency : SWEEP_FREQUENCIES[0];
    subghz_devices_set_frequency(ctx->device, freq);
    subghz_devices_set_rx(ctx->device);

    ctx->running = true;
    return true;
}

void signal_capture_stop(SignalCaptureCtx* ctx) {
    if(!ctx->running || !ctx->device) return;
    if(s_raw.capturing) {
        subghz_devices_stop_async_rx(ctx->device);
        s_raw.capturing = false;
    }
    subghz_devices_idle(ctx->device);
    subghz_devices_sleep(ctx->device);
    subghz_devices_end(ctx->device);
    ctx->device  = NULL;
    ctx->running = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// RSSI polling
// ─────────────────────────────────────────────────────────────────────────────

float signal_capture_poll_rssi(SignalCaptureCtx* ctx) {
    if(!ctx->running || !ctx->device) return -120.0f;
    return subghz_devices_get_rssi(ctx->device);
}

float signal_capture_noise_floor(SignalCaptureCtx* ctx) {
    if(!ctx->device) return -90.0f;
    float    total   = 0.0f;
    uint16_t samples = 0;
    uint8_t  limit   = 8;

    subghz_devices_idle(ctx->device);
    for(uint16_t i = 0; i < SWEEP_FREQ_COUNT && i < limit; i++) {
        subghz_devices_set_frequency(ctx->device, SWEEP_FREQUENCIES[i]);
        subghz_devices_set_rx(ctx->device);
        furi_delay_ms(20);
        total += subghz_devices_get_rssi(ctx->device);
        samples++;
        subghz_devices_idle(ctx->device);
    }

    // Restore original frequency and RX mode
    uint32_t freq = ctx->frequency > 0 ? ctx->frequency : SWEEP_FREQUENCIES[0];
    subghz_devices_set_frequency(ctx->device, freq);
    subghz_devices_set_rx(ctx->device);

    return samples > 0 ? total / (float)samples : -90.0f;
}

// ─────────────────────────────────────────────────────────────────────────────
// Feature extraction
// ─────────────────────────────────────────────────────────────────────────────

static Modulation detect_modulation(const uint32_t* pulses, uint16_t count) {
    if(count < 4) return ModulationUnknown;

    uint32_t min_p = pulses[0];
    uint32_t max_p = pulses[0];
    for(uint16_t i = 0; i < count; i++) {
        if(pulses[i] < min_p) min_p = pulses[i];
        if(pulses[i] > max_p) max_p = pulses[i];
    }

    float ratio = (min_p > 0) ? (float)max_p / (float)min_p : 10.0f;
    if(ratio >= 2.0f)  return ModulationOOK;
    if(ratio >= 1.2f)  return ModulationGFSK;
    return ModulationFSK;
}

static uint16_t estimate_bandwidth(ScanMode mode) {
    if(mode == ScanModeRFNarrow) return 30;
    if(mode == ScanModeRFWide)   return 200;
    return 650;
}

static bool detect_repetition(const uint32_t* pulses, uint16_t count, uint8_t* repeat_out) {
    uint8_t gaps = 0;
    for(uint16_t i = 0; i < count; i++) {
        if(pulses[i] > 10000) gaps++;
    }
    *repeat_out = gaps;
    return gaps > 0;
}

static bool is_fixed_code_heuristic(const uint32_t* pulses, uint16_t count) {
    if(count < 32) return false;
    uint64_t sum = 0;
    for(uint16_t i = 0; i < count; i++) sum += pulses[i];
    uint32_t avg = (uint32_t)(sum / count);
    uint32_t var = 0;
    for(uint16_t i = 0; i < count; i++) {
        int32_t d = (int32_t)pulses[i] - (int32_t)avg;
        var += (uint32_t)(d < 0 ? -d : d);
    }
    var /= count;
    return var < avg / 4;
}

// ─────────────────────────────────────────────────────────────────────────────
// Full acquisition
// ─────────────────────────────────────────────────────────────────────────────

bool signal_capture_acquire(SignalCaptureCtx* ctx, SignalCapture* out) {
    if(!ctx->running) return false;
    memset(out, 0, sizeof(SignalCapture));

    out->frequency   = ctx->frequency > 0 ? ctx->frequency : signal_capture_current_freq(ctx);
    out->rssi        = subghz_devices_get_rssi(ctx->device);
    out->noise_floor = signal_capture_noise_floor(ctx);
    out->snr         = out->rssi - out->noise_floor;

    memset(&s_raw, 0, sizeof(s_raw));
    s_raw.capturing = true;
    subghz_devices_start_async_rx(ctx->device, rx_callback, NULL);
    furi_delay_ms(CAPTURE_TIMEOUT_MS);
    subghz_devices_stop_async_rx(ctx->device);
    s_raw.capturing = false;

    if(s_raw.count < MIN_PULSE_COUNT) return false;

    uint16_t copy_count = s_raw.count < 512 ? s_raw.count : 512;
    memcpy(out->raw_pulses, s_raw.pulses, copy_count * sizeof(uint32_t));
    out->pulse_count   = copy_count;
    out->timestamp     = furi_get_tick();

    uint32_t min_p = out->raw_pulses[0];
    uint32_t max_p = out->raw_pulses[0];
    uint64_t sum   = 0;
    uint16_t valid = 0;
    for(uint16_t i = 0; i < copy_count; i++) {
        uint32_t p = out->raw_pulses[i];
        if(p > 10000) continue;
        if(p < min_p) min_p = p;
        if(p > max_p) max_p = p;
        sum += p;
        valid++;
    }
    out->pulse_min = (uint16_t)(min_p > 65535 ? 65535 : min_p);
    out->pulse_max = (uint16_t)(max_p > 65535 ? 65535 : max_p);
    out->pulse_avg = (uint16_t)(valid > 0 ? sum / valid : 0);

    out->modulation        = detect_modulation(out->raw_pulses, copy_count);
    out->bandwidth_khz     = estimate_bandwidth(ctx->mode);
    out->packet_bits       = (uint16_t)(copy_count / 2);
    out->repeating         = detect_repetition(out->raw_pulses, copy_count, &out->repeat_count);
    out->fixed_code_likely = is_fixed_code_heuristic(out->raw_pulses, copy_count);

    subghz_devices_set_rx(ctx->device);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Sweep
// ─────────────────────────────────────────────────────────────────────────────

uint32_t signal_capture_next_freq(SignalCaptureCtx* ctx) {
    ctx->sweep_index = (ctx->sweep_index + 1) % SWEEP_FREQ_COUNT;
    uint32_t freq    = SWEEP_FREQUENCIES[ctx->sweep_index];
    if(ctx->device && ctx->running) {
        subghz_devices_idle(ctx->device);
        subghz_devices_set_frequency(ctx->device, freq);
        subghz_devices_set_rx(ctx->device);
    }
    return freq;
}

uint32_t signal_capture_current_freq(SignalCaptureCtx* ctx) {
    if(ctx->frequency > 0) return ctx->frequency;
    return SWEEP_FREQUENCIES[ctx->sweep_index];
}
bool signal_capture_antenna_ok(const SignalCaptureCtx* ctx) { return ctx->antenna_ok; }
