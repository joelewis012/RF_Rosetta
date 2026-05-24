#include "signal_capture.h"
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_subghz.h>
#include <furi_hal_gpio.h>
#include <string.h>

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
    ScanMode    mode;
    AntennaMode antenna;
    uint32_t    frequency;
    float       threshold;
    uint16_t    sweep_index;
    bool        running;
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

// ─────────────────────────────────────────────────────────────────────────────
// Antenna switching via GPIO
// gpio_ext_pa7 = Flipper external header pin 10 (SPI MOSI / GPIO)
// If your hardware uses a different pin, update RF_ROSETTA_ANTENNA_GPIO_PIN
// in signal_capture.h
// ─────────────────────────────────────────────────────────────────────────────

static void apply_antenna(AntennaMode ant) {
    // gpio_ext_pa7 is declared in furi_hal_gpio.h (included via furi_hal.h)
    // It maps to Flipper external GPIO header pin 10.
    const GpioPin* pin = RF_ROSETTA_ANTENNA_GPIO_PIN;
    if(ant == AntennaExternal) {
        furi_hal_gpio_init(pin, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
        furi_hal_gpio_write(pin, true);
    } else {
        furi_hal_gpio_init(pin, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
        furi_hal_gpio_write(pin, false);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Context lifecycle
// ─────────────────────────────────────────────────────────────────────────────

SignalCaptureCtx* signal_capture_alloc(void) {
    SignalCaptureCtx* ctx = malloc(sizeof(SignalCaptureCtx));
    furi_assert(ctx);
    ctx->mode         = ScanModeSubGHz;
    ctx->antenna      = AntennaInternal;
    ctx->frequency    = 0;
    ctx->threshold    = SIGNAL_THRESHOLD_DBM;
    ctx->sweep_index  = 0;
    ctx->running      = false;
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

    furi_hal_subghz_reset();
    furi_hal_subghz_idle();
    apply_antenna(ctx->antenna);

    // Apply mode-specific CC1101 preset using furi_hal_subghz_load_custom_preset.
    // Each array is {register_address, value} pairs terminated by {0,0}.
    // OOK650: standard OOK, 650 kHz BW — best for remotes and sensors
    // FSK238: narrow 2-FSK, 238 kHz deviation — better sensitivity for FSK
    // FSK476: wider 2-FSK, 476 kHz deviation — catches more FSK signal types
    static const uint8_t preset_ook650[] = {
        0x02, 0x0D, 0x03, 0x07, 0x08, 0x32, 0x0B, 0x06,
        0x14, 0x00, 0x13, 0x00, 0x12, 0x30, 0x11, 0x32,
        0x10, 0x17, 0x18, 0x18, 0x19, 0x18, 0x1D, 0x91,
        0x1C, 0x00, 0x1B, 0x07, 0x00, 0x00,
    };
    static const uint8_t preset_fsk238[] = {
        0x02, 0x0D, 0x03, 0x07, 0x08, 0x32, 0x0B, 0x06,
        0x14, 0x00, 0x13, 0x00, 0x12, 0x0C, 0x11, 0x32,
        0x10, 0x17, 0x18, 0x18, 0x19, 0x18, 0x1D, 0x91,
        0x1C, 0x00, 0x1B, 0x07, 0x00, 0x00,
    };
    static const uint8_t preset_fsk476[] = {
        0x02, 0x0D, 0x03, 0x07, 0x08, 0x32, 0x0B, 0x06,
        0x14, 0x00, 0x13, 0x00, 0x12, 0x0E, 0x11, 0x32,
        0x10, 0x17, 0x18, 0x18, 0x19, 0x18, 0x1D, 0x91,
        0x1C, 0x00, 0x1B, 0x07, 0x00, 0x00,
    };

    switch(ctx->mode) {
        case ScanModeRFNarrow:
            furi_hal_subghz_load_custom_preset(preset_fsk238);
            break;
        case ScanModeRFWide:
            furi_hal_subghz_load_custom_preset(preset_fsk476);
            break;
        case ScanModeSubGHz:
        default:
            furi_hal_subghz_load_custom_preset(preset_ook650);
            break;
    }

    uint32_t freq = ctx->frequency > 0 ? ctx->frequency : SWEEP_FREQUENCIES[0];
    furi_hal_subghz_set_frequency_and_path(freq);
    furi_hal_subghz_rx();
    ctx->running = true;
    return true;
}

void signal_capture_stop(SignalCaptureCtx* ctx) {
    if(!ctx->running) return;
    if(s_raw.capturing) {
        furi_hal_subghz_stop_async_rx();
        s_raw.capturing = false;
    }
    furi_hal_subghz_idle();
    furi_hal_subghz_sleep();
    ctx->running = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// RSSI polling
// ─────────────────────────────────────────────────────────────────────────────

float signal_capture_poll_rssi(SignalCaptureCtx* ctx) {
    if(!ctx->running) return -120.0f;
    return furi_hal_subghz_get_rssi();
}

float signal_capture_noise_floor(SignalCaptureCtx* ctx) {
    float    total   = 0.0f;
    uint16_t samples = 0;
    uint8_t  limit   = 8;

    furi_hal_subghz_idle();
    for(uint16_t i = 0; i < SWEEP_FREQ_COUNT && i < limit; i++) {
        furi_hal_subghz_set_frequency_and_path(SWEEP_FREQUENCIES[i]);
        furi_hal_subghz_rx();
        furi_delay_ms(20);
        total += furi_hal_subghz_get_rssi();
        samples++;
        furi_hal_subghz_idle();
    }

    uint32_t freq = ctx->frequency > 0 ? ctx->frequency : SWEEP_FREQUENCIES[0];
    furi_hal_subghz_set_frequency_and_path(freq);
    furi_hal_subghz_rx();

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
    out->rssi        = furi_hal_subghz_get_rssi();
    out->noise_floor = signal_capture_noise_floor(ctx);
    out->snr         = out->rssi - out->noise_floor;

    memset(&s_raw, 0, sizeof(s_raw));
    s_raw.capturing = true;
    furi_hal_subghz_start_async_rx(rx_callback, NULL);
    furi_delay_ms(CAPTURE_TIMEOUT_MS);
    furi_hal_subghz_stop_async_rx();
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

    furi_hal_subghz_rx();
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Sweep
// ─────────────────────────────────────────────────────────────────────────────

uint32_t signal_capture_next_freq(SignalCaptureCtx* ctx) {
    ctx->sweep_index = (ctx->sweep_index + 1) % SWEEP_FREQ_COUNT;
    uint32_t freq    = SWEEP_FREQUENCIES[ctx->sweep_index];
    furi_hal_subghz_idle();
    furi_hal_subghz_set_frequency_and_path(freq);
    furi_hal_subghz_rx();
    return freq;
}

uint32_t signal_capture_current_freq(SignalCaptureCtx* ctx) {
    if(ctx->frequency > 0) return ctx->frequency;
    return SWEEP_FREQUENCIES[ctx->sweep_index];
}
