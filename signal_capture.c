#include "signal_capture.h"
#include <furi.h>
#include <furi_hal_subghz.h>
#include <furi_hal_gpio.h>
#include <string.h>
#include <math.h>

// ─────────────────────────────────────────────────────────────────────────────
// Frequency sweep table — covers all major ISM sub-bands
// ─────────────────────────────────────────────────────────────────────────────

const uint32_t SWEEP_FREQUENCIES[] = {
    // 315 MHz band (North America — TPMS, key fobs)
    314900000, 315100000,
    // 433 MHz band (global ISM — the busiest band)
    433050000, 433420000, 433920000, 434420000, 434790000,
    // 434 MHz
    434000000,
    // 400–406 MHz (radiosonde, VHF)
    401000000, 403000000, 405000000,
    // 148–154 MHz (wildlife tags, VHF beacons)
    149500000, 151000000, 152000000,
    // 868 MHz band (European ISM — Z-Wave, meters, LoRa)
    868000000, 868300000, 868500000, 869000000, 869525000,
    // 902–928 MHz (North American ISM — Z-Wave US, LoRa US)
    902000000, 908000000, 915000000, 920000000, 928000000,
};

const uint16_t SWEEP_FREQ_COUNT = sizeof(SWEEP_FREQUENCIES) / sizeof(SWEEP_FREQUENCIES[0]);

// ─────────────────────────────────────────────────────────────────────────────
// Raw pulse capture buffer (filled by CC1101 interrupt callback)
// ─────────────────────────────────────────────────────────────────────────────

#define RAW_BUF_SIZE 512
#define CAPTURE_TIMEOUT_MS 400  // max capture window
#define MIN_PULSE_COUNT 16      // discard shorter captures as noise
#define SIGNAL_THRESHOLD_DBM -80.0f

typedef struct {
    uint32_t pulses[RAW_BUF_SIZE];
    uint16_t count;
    bool     overflow;
    bool     capturing;
} RawBuffer;

static RawBuffer s_raw; // single global buffer — only one capture at a time

// CC1101 async RX callback — called from ISR context on each signal edge
static void rx_callback(bool level, uint32_t duration, void* context) {
    UNUSED(level);
    UNUSED(context);
    if(!s_raw.capturing) return;
    if(s_raw.count < RAW_BUF_SIZE) {
        s_raw.pulses[s_raw.count++] = duration;
    } else {
        s_raw.overflow = true;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Context
// ─────────────────────────────────────────────────────────────────────────────

struct SignalCaptureCtx {
    ScanMode    mode;
    AntennaMode antenna;
    uint32_t    frequency;       // 0 = sweep
    float       threshold;       // dBm
    uint16_t    sweep_index;
    bool        radio_acquired;
    bool        running;
};

// ─────────────────────────────────────────────────────────────────────────────
// RSSI History
// ─────────────────────────────────────────────────────────────────────────────

void rssi_history_push(RSSIHistory* h, float rssi) {
    h->values[h->head] = rssi;
    h->head = (h->head + 1) % RSSI_HISTORY_LEN;
    if(h->count < RSSI_HISTORY_LEN) h->count++;
}

float rssi_history_get(const RSSIHistory* h, uint8_t index) {
    if(index >= h->count) return -120.0f;
    // oldest entry is at (head - count + index) mod LEN
    uint8_t pos = (uint8_t)((h->head - h->count + index + RSSI_HISTORY_LEN) % RSSI_HISTORY_LEN);
    return h->values[pos];
}

// ─────────────────────────────────────────────────────────────────────────────
// Internal helpers
// ─────────────────────────────────────────────────────────────────────────────

static void apply_antenna(AntennaMode ant) {
    if(ant == AntennaExternal) {
        furi_hal_gpio_init(RF_ROSETTA_ANTENNA_GPIO_PIN, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
        furi_hal_gpio_write(RF_ROSETTA_ANTENNA_GPIO_PIN, true);
    } else {
        if(furi_hal_gpio_read(RF_ROSETTA_ANTENNA_GPIO_PIN)) {
            furi_hal_gpio_write(RF_ROSETTA_ANTENNA_GPIO_PIN, false);
        }
    }
}

// Set CC1101 bandwidth preset based on scan mode
static void apply_mode_preset(ScanMode mode) {
    switch(mode) {
        case ScanModeSubGHz:
            // Standard OOK async preset — widest compatibility
            furi_hal_subghz_load_preset(FuriHalSubGhzPresetOok650Async);
            break;
        case ScanModeRFNarrow:
            // Narrow FSK — better sensitivity, good for TPMS, meters
            furi_hal_subghz_load_preset(FuriHalSubGhzPreset2FSKDev2_38KhzAsync);
            break;
        case ScanModeRFWide:
            // Wide FSK — catches Z-Wave, LoRa preambles, wideband signals
            furi_hal_subghz_load_preset(FuriHalSubGhzPreset2FSKDev47_6KhzAsync);
            break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Context lifecycle
// ─────────────────────────────────────────────────────────────────────────────

SignalCaptureCtx* signal_capture_alloc(void) {
    SignalCaptureCtx* ctx = malloc(sizeof(SignalCaptureCtx));
    furi_assert(ctx);
    ctx->mode           = ScanModeSubGHz;
    ctx->antenna        = AntennaInternal;
    ctx->frequency      = 0;
    ctx->threshold      = SIGNAL_THRESHOLD_DBM;
    ctx->sweep_index    = 0;
    ctx->radio_acquired = false;
    ctx->running        = false;
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
    ctx->antenna = antenna;
    apply_antenna(antenna);
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

    if(!furi_hal_subghz_is_occupied()) {
        furi_hal_subghz_reset();
        furi_hal_subghz_idle();
        apply_mode_preset(ctx->mode);
        apply_antenna(ctx->antenna);

        uint32_t freq = ctx->frequency > 0 ? ctx->frequency : SWEEP_FREQUENCIES[0];
        furi_hal_subghz_set_frequency_and_path(freq);
        furi_hal_subghz_rx();
        ctx->radio_acquired = true;
        ctx->running        = true;
        return true;
    }
    return false;
}

void signal_capture_stop(SignalCaptureCtx* ctx) {
    if(!ctx->running) return;
    if(s_raw.capturing) {
        furi_hal_subghz_stop_async_rx();
        s_raw.capturing = false;
    }
    furi_hal_subghz_idle();
    furi_hal_subghz_sleep();
    ctx->radio_acquired = false;
    ctx->running        = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// RSSI polling
// ─────────────────────────────────────────────────────────────────────────────

float signal_capture_poll_rssi(SignalCaptureCtx* ctx) {
    if(!ctx->running) return -120.0f;
    return furi_hal_subghz_get_rssi();
}

float signal_capture_noise_floor(SignalCaptureCtx* ctx) {
    // Sweep through all frequencies quickly and take the lowest RSSI as floor
    float floor_val = 0.0f;
    uint16_t samples = 0;
    furi_hal_subghz_idle();
    apply_mode_preset(ctx->mode);
    for(uint16_t i = 0; i < SWEEP_FREQ_COUNT && i < 8; i++) {
        furi_hal_subghz_set_frequency_and_path(SWEEP_FREQUENCIES[i]);
        furi_hal_subghz_rx();
        furi_delay_ms(20);
        float r = furi_hal_subghz_get_rssi();
        floor_val += r;
        samples++;
        furi_hal_subghz_idle();
    }
    // Restore original freq
    uint32_t freq = ctx->frequency > 0 ? ctx->frequency : SWEEP_FREQUENCIES[0];
    furi_hal_subghz_set_frequency_and_path(freq);
    furi_hal_subghz_rx();
    return samples > 0 ? floor_val / (float)samples : -90.0f;
}

// ─────────────────────────────────────────────────────────────────────────────
// Feature extraction from raw pulses
// ─────────────────────────────────────────────────────────────────────────────

static Modulation detect_modulation(const uint32_t* pulses, uint16_t count) {
    if(count < 4) return ModulationUnknown;

    // OOK: alternating mark/space with a ratio > 2:1 between long and short pulses
    uint32_t min_p = pulses[0], max_p = pulses[0];
    uint64_t sum   = 0;
    for(uint16_t i = 0; i < count; i++) {
        if(pulses[i] < min_p) min_p = pulses[i];
        if(pulses[i] > max_p) max_p = pulses[i];
        sum += pulses[i];
    }
    uint32_t avg = (uint32_t)(sum / count);

    float ratio = (min_p > 0) ? (float)max_p / (float)min_p : 10.0f;

    if(ratio >= 2.0f) return ModulationOOK;
    if(ratio >= 1.2f && avg < 200) return ModulationGFSK;
    return ModulationFSK;
}

static uint16_t estimate_bandwidth(ScanMode mode) {
    switch(mode) {
        case ScanModeSubGHz:   return 650;
        case ScanModeRFNarrow: return 30;
        case ScanModeRFWide:   return 200;
        default:               return 100;
    }
}

static bool detect_repetition(const uint32_t* pulses, uint16_t count, uint8_t* repeat_out) {
    // Look for a large gap (>10ms) which typically separates packet repetitions
    uint8_t gaps = 0;
    for(uint16_t i = 0; i < count; i++) {
        if(pulses[i] > 10000) gaps++;
    }
    *repeat_out = gaps;
    return gaps > 0;
}

static bool is_fixed_code_heuristic(const uint32_t* pulses, uint16_t count) {
    // Fixed codes usually have very consistent pulse widths across repetitions
    if(count < 32) return false;
    uint64_t sum = 0;
    for(uint16_t i = 0; i < count; i++) sum += pulses[i];
    uint32_t avg = (uint32_t)(sum / count);
    uint32_t variance = 0;
    for(uint16_t i = 0; i < count; i++) {
        int32_t d = (int32_t)pulses[i] - (int32_t)avg;
        variance += (uint32_t)(d < 0 ? -d : d);
    }
    variance /= count;
    // Low variance relative to average = consistent = likely fixed code
    return variance < avg / 4;
}

// ─────────────────────────────────────────────────────────────────────────────
// Full signal acquisition
// ─────────────────────────────────────────────────────────────────────────────

bool signal_capture_acquire(SignalCaptureCtx* ctx, SignalCapture* out) {
    if(!ctx->running) return false;
    memset(out, 0, sizeof(SignalCapture));

    // Record frequency and initial RSSI
    out->frequency = ctx->frequency > 0 ? ctx->frequency : signal_capture_current_freq(ctx);
    out->rssi      = furi_hal_subghz_get_rssi();

    // Measure noise floor briefly
    out->noise_floor = signal_capture_noise_floor(ctx);
    out->snr         = out->rssi - out->noise_floor;

    // Start raw async capture
    memset(&s_raw, 0, sizeof(s_raw));
    s_raw.capturing = true;
    furi_hal_subghz_start_async_rx(rx_callback, NULL);

    // Wait for capture window
    furi_delay_ms(CAPTURE_TIMEOUT_MS);

    // Stop capture
    furi_hal_subghz_stop_async_rx();
    s_raw.capturing = false;

    if(s_raw.count < MIN_PULSE_COUNT) return false;

    // Copy raw pulses
    uint16_t copy_count = s_raw.count < 512 ? s_raw.count : 512;
    memcpy(out->raw_pulses, s_raw.pulses, copy_count * sizeof(uint32_t));
    out->pulse_count = copy_count;
    out->timestamp   = furi_get_tick();

    // Feature extraction
    uint32_t min_p = out->raw_pulses[0];
    uint32_t max_p = out->raw_pulses[0];
    uint64_t sum   = 0;
    for(uint16_t i = 0; i < copy_count; i++) {
        uint32_t p = out->raw_pulses[i];
        if(p > 10000) continue; // skip inter-packet gaps for stats
        if(p < min_p) min_p = p;
        if(p > max_p) max_p = p;
        sum += p;
    }
    out->pulse_min = (uint16_t)(min_p > 65535 ? 65535 : min_p);
    out->pulse_max = (uint16_t)(max_p > 65535 ? 65535 : max_p);
    out->pulse_avg = (uint16_t)(copy_count > 0 ? sum / copy_count : 0);

    out->modulation    = detect_modulation(out->raw_pulses, copy_count);
    out->bandwidth_khz = estimate_bandwidth(ctx->mode);
    out->packet_bits   = (uint16_t)(copy_count / 2); // rough: 2 pulses per bit
    out->repeating     = detect_repetition(out->raw_pulses, copy_count, &out->repeat_count);
    out->fixed_code_likely = is_fixed_code_heuristic(out->raw_pulses, copy_count);

    // Restart RX for next capture
    furi_hal_subghz_rx();
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Frequency sweep
// ─────────────────────────────────────────────────────────────────────────────

uint32_t signal_capture_next_freq(SignalCaptureCtx* ctx) {
    ctx->sweep_index = (ctx->sweep_index + 1) % SWEEP_FREQ_COUNT;
    uint32_t freq    = SWEEP_FREQUENCIES[ctx->sweep_index];
    furi_hal_subghz_idle();
    apply_mode_preset(ctx->mode);
    furi_hal_subghz_set_frequency_and_path(freq);
    furi_hal_subghz_rx();
    return freq;
}

uint32_t signal_capture_current_freq(SignalCaptureCtx* ctx) {
    if(ctx->frequency > 0) return ctx->frequency;
    return SWEEP_FREQUENCIES[ctx->sweep_index];
}
