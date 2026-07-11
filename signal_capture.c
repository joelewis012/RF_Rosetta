#include "signal_capture.h"
#include "cc1101_ext.h"
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_subghz.h>
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
    ScanMode     mode;
    AntennaMode  antenna;
    bool         antenna_ok;
    bool         use_ext_cc1101;
    uint32_t     frequency;
    float        threshold;
    uint16_t     sweep_index;
    bool         running;
    uint16_t     dwell_ms;
    uint8_t      preset_idx;
    RFGPIOConfig gpio;   // active GPIO config for external CC1101
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

// (Antenna selection: internal uses furi_hal_subghz, external uses cc1101_ext bit-bang)

// ─────────────────────────────────────────────────────────────────────────────
// Context lifecycle
// ─────────────────────────────────────────────────────────────────────────────

SignalCaptureCtx* signal_capture_alloc(void) {
    SignalCaptureCtx* ctx = malloc(sizeof(SignalCaptureCtx));
    furi_assert(ctx);
    ctx->mode             = ScanModeAll;
    ctx->antenna          = AntennaInternal;
    ctx->antenna_ok       = true;
    ctx->use_ext_cc1101   = false;
    ctx->frequency        = 0;
    ctx->threshold    = SIGNAL_THRESHOLD_DBM;
    ctx->sweep_index  = 0;
    ctx->running      = false;
    ctx->dwell_ms     = 300;
    ctx->preset_idx   = 0;
    // Default GPIO config = 3-in-1 board pinout (caller can override with signal_capture_set_gpio)
    ctx->gpio.mosi = &gpio_ext_pa7;
    ctx->gpio.miso = &gpio_ext_pa6;
    ctx->gpio.csn  = &gpio_ext_pa4;
    ctx->gpio.sck  = &gpio_ext_pb3;
    ctx->gpio.aux  = &gpio_ext_pb2;
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

void signal_capture_set_gpio(SignalCaptureCtx* ctx, RFGPIOConfig gpio) {
    ctx->gpio = gpio;
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

    static const uint8_t preset_ook650[] = {
        0x02,0x0D, 0x03,0x07, 0x08,0x32, 0x0B,0x06,
        0x14,0x00, 0x13,0x00, 0x12,0x30, 0x11,0x32,
        0x10,0x17, 0x18,0x18, 0x19,0x18, 0x1D,0x91,
        0x1C,0x00, 0x1B,0x07, 0x00,0x00,
    };
    static const uint8_t preset_fsk238[] = {
        0x02,0x0D, 0x03,0x07, 0x08,0x32, 0x0B,0x06,
        0x14,0x00, 0x13,0x00, 0x12,0x0C, 0x11,0x32,
        0x10,0x17, 0x18,0x18, 0x19,0x18, 0x1D,0x91,
        0x1C,0x00, 0x1B,0x07, 0x00,0x00,
    };
    static const uint8_t preset_fsk476[] = {
        0x02,0x0D, 0x03,0x07, 0x08,0x32, 0x0B,0x06,
        0x14,0x00, 0x13,0x00, 0x12,0x0E, 0x11,0x32,
        0x10,0x17, 0x18,0x18, 0x19,0x18, 0x1D,0x91,
        0x1C,0x00, 0x1B,0x07, 0x00,0x00,
    };

    ctx->preset_idx      = 0;
    ctx->use_ext_cc1101  = false;
    ctx->antenna_ok      = true;

    uint32_t freq = ctx->frequency > 0 ? ctx->frequency : SWEEP_FREQUENCIES[0];

    if(ctx->antenna == AntennaExternal) {
        // ── Try external CC1101 via bit-bang SPI ─────────────────────────────
        if(cc1101_ext_init(&ctx->gpio)) {
            // External chip confirmed — use bit-bang driver
            ctx->use_ext_cc1101 = true;
            const uint8_t* preset = preset_ook650;
            if(ctx->mode == ScanModeRFNarrow) preset = preset_fsk238;
            if(ctx->mode == ScanModeRFWide)   preset = preset_fsk476;
            cc1101_ext_load_preset(&ctx->gpio, preset);
            cc1101_ext_set_frequency(&ctx->gpio, freq);
            cc1101_ext_rx(&ctx->gpio);
            ctx->running = true;
            return true;
        } else {
            // External not found — fall back to internal, flag it
            cc1101_ext_deinit(&ctx->gpio);
            ctx->antenna_ok = false;
        }
    }

    // ── Internal CC1101 via furi_hal_subghz ──────────────────────────────────
    furi_hal_subghz_reset();
    furi_hal_subghz_idle();

    switch(ctx->mode) {
        case ScanModeAll:
        case ScanModeSubGHz:   furi_hal_subghz_load_custom_preset(preset_ook650); break;
        case ScanModeRFNarrow: furi_hal_subghz_load_custom_preset(preset_fsk238); break;
        case ScanModeRFWide:   furi_hal_subghz_load_custom_preset(preset_fsk476); break;
    }

    furi_hal_subghz_set_frequency_and_path(freq);
    furi_hal_subghz_rx();
    ctx->running = true;
    return true;
}

void signal_capture_stop(SignalCaptureCtx* ctx) {
    if(!ctx->running) return;
    if(ctx->use_ext_cc1101) {
        cc1101_ext_idle(&ctx->gpio);
        cc1101_ext_deinit(&ctx->gpio);
    } else {
        if(s_raw.capturing) {
            furi_hal_subghz_stop_async_rx();
            s_raw.capturing = false;
        }
        furi_hal_subghz_idle();
        furi_hal_subghz_sleep();
    }
    ctx->use_ext_cc1101 = false;
    ctx->running = false;
}

float signal_capture_poll_rssi(SignalCaptureCtx* ctx) {
    if(!ctx->running) return -120.0f;

    if(ctx->use_ext_cc1101) {
        return cc1101_ext_get_rssi(&ctx->gpio);
    }

    if(ctx->mode == ScanModeAll) {
        static const uint8_t preset_ook650[] = {
            0x02,0x0D, 0x03,0x07, 0x08,0x32, 0x0B,0x06,
            0x14,0x00, 0x13,0x00, 0x12,0x30, 0x11,0x32,
            0x10,0x17, 0x18,0x18, 0x19,0x18, 0x1D,0x91,
            0x1C,0x00, 0x1B,0x07, 0x00,0x00,
        };
        static const uint8_t preset_fsk238[] = {
            0x02,0x0D, 0x03,0x07, 0x08,0x32, 0x0B,0x06,
            0x14,0x00, 0x13,0x00, 0x12,0x0C, 0x11,0x32,
            0x10,0x17, 0x18,0x18, 0x19,0x18, 0x1D,0x91,
            0x1C,0x00, 0x1B,0x07, 0x00,0x00,
        };
        static const uint8_t preset_fsk476[] = {
            0x02,0x0D, 0x03,0x07, 0x08,0x32, 0x0B,0x06,
            0x14,0x00, 0x13,0x00, 0x12,0x0E, 0x11,0x32,
            0x10,0x17, 0x18,0x18, 0x19,0x18, 0x1D,0x91,
            0x1C,0x00, 0x1B,0x07, 0x00,0x00,
        };
        static const uint8_t* presets[3] = { preset_ook650, preset_fsk238, preset_fsk476 };
        ctx->preset_idx = (ctx->preset_idx + 1) % 3;
        uint32_t freq = ctx->frequency > 0 ? ctx->frequency : SWEEP_FREQUENCIES[ctx->sweep_index];
        furi_hal_subghz_idle();
        furi_hal_subghz_load_custom_preset(presets[ctx->preset_idx]);
        furi_hal_subghz_set_frequency_and_path(freq);
        furi_hal_subghz_rx();
        furi_delay_ms(8);
    }

    return furi_hal_subghz_get_rssi();
}

float signal_capture_noise_floor(SignalCaptureCtx* ctx) {
    // External CC1101: can't sweep internal HAL, return fixed estimate
    if(ctx->use_ext_cc1101) return -90.0f;

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

    if(ctx->use_ext_cc1101) {
        // External CC1101: capture pulses via GDO0 polling (PB2)
        out->rssi        = cc1101_ext_get_rssi(&ctx->gpio);
        out->noise_floor = -90.0f;
        out->snr         = out->rssi - out->noise_floor;

        uint16_t count = cc1101_ext_capture_pulses(
            &ctx->gpio, out->raw_pulses, 512, CAPTURE_TIMEOUT_MS);

        if(count < MIN_PULSE_COUNT) {
            cc1101_ext_rx(&ctx->gpio);
            return false;
        }

        out->pulse_count = count;
        out->timestamp   = furi_get_tick();

        uint32_t mn = out->raw_pulses[0], mx = out->raw_pulses[0];
        uint64_t sm = 0; uint16_t vld = 0;
        for(uint16_t i = 0; i < count; i++) {
            uint32_t p = out->raw_pulses[i];
            if(p > 10000) continue;
            if(p < mn) mn = p;
            if(p > mx) mx = p;
            sm += p; vld++;
        }
        out->pulse_min         = (uint16_t)(mn > 65535 ? 65535 : mn);
        out->pulse_max         = (uint16_t)(mx > 65535 ? 65535 : mx);
        out->pulse_avg         = (uint16_t)(vld > 0 ? sm / vld : 0);
        out->modulation        = detect_modulation(out->raw_pulses, count);
        out->bandwidth_khz     = estimate_bandwidth(ctx->mode);
        out->packet_bits       = (uint16_t)(count / 2);
        out->repeating         = detect_repetition(out->raw_pulses, count, &out->repeat_count);
        out->fixed_code_likely = is_fixed_code_heuristic(out->raw_pulses, count);

        cc1101_ext_rx(&ctx->gpio);
        return true;
    }

    // Internal CC1101 via furi_hal async RX
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
    out->pulse_min         = (uint16_t)(min_p > 65535 ? 65535 : min_p);
    out->pulse_max         = (uint16_t)(max_p > 65535 ? 65535 : max_p);
    out->pulse_avg         = (uint16_t)(valid > 0 ? sum / valid : 0);
    out->modulation        = detect_modulation(out->raw_pulses, copy_count);
    out->bandwidth_khz     = estimate_bandwidth(ctx->mode);
    out->packet_bits       = (uint16_t)(copy_count / 2);
    out->repeating         = detect_repetition(out->raw_pulses, copy_count, &out->repeat_count);
    out->fixed_code_likely = is_fixed_code_heuristic(out->raw_pulses, copy_count);

    furi_hal_subghz_rx();
    return true;
}

uint32_t signal_capture_next_freq(SignalCaptureCtx* ctx) {
    ctx->sweep_index = (ctx->sweep_index + 1) % SWEEP_FREQ_COUNT;
    uint32_t freq    = SWEEP_FREQUENCIES[ctx->sweep_index];
    if(ctx->use_ext_cc1101) {
        cc1101_ext_idle(&ctx->gpio);
        cc1101_ext_set_frequency(&ctx->gpio, freq);
        cc1101_ext_rx(&ctx->gpio);
    } else {
        furi_hal_subghz_idle();
        furi_hal_subghz_set_frequency_and_path(freq);
        furi_hal_subghz_rx();
    }
    return freq;
}

uint32_t signal_capture_current_freq(SignalCaptureCtx* ctx) {
    if(ctx->frequency > 0) return ctx->frequency;
    return SWEEP_FREQUENCIES[ctx->sweep_index];
}
void signal_capture_set_dwell(SignalCaptureCtx* ctx, uint16_t dwell_ms) {
    ctx->dwell_ms = dwell_ms < 50 ? 50 : (dwell_ms > 3000 ? 3000 : dwell_ms);
}

uint16_t signal_capture_get_dwell(const SignalCaptureCtx* ctx) {
    return ctx->dwell_ms;
}

bool signal_capture_antenna_ok(const SignalCaptureCtx* ctx) {
    return ctx->antenna_ok;
}
