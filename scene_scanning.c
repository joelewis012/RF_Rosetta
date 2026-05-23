#include "rf_rosetta.h"
#include <gui/canvas.h>
#include <gui/elements.h>
#include <string.h>
#include <stdio.h>

// ─────────────────────────────────────────────────────────────────────────────
// View model (shared between draw callback and timer)
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    float    rssi;
    float    noise_floor;
    float    snr;
    float    history[RSSI_HISTORY_LEN];
    uint8_t  history_count;
    uint32_t frequency;
    bool     signal_detected;
    bool     analyzing;
    ScanMode mode;
    bool     antenna_external;
    char     freq_str[20];
    char     status_str[32];
    uint8_t  anim_tick; // drives the listening waveform animation
} ScanViewModel;

// ─────────────────────────────────────────────────────────────────────────────
// RSSI → bar height (1–5 bars), screen coords on 128×64 display
// ─────────────────────────────────────────────────────────────────────────────

#define BAR_COUNT    5
#define BAR_W        10
#define BAR_GAP      4
#define BAR_MAX_H    30
#define BAR_BASE_Y   50  // y of bar bottom edge
#define BAR_START_X  28  // x of first bar

static uint8_t rssi_to_bars(float rssi) {
    // Maps -100 dBm → 1 bar, -40 dBm → 5 bars
    if(rssi <= -100.0f) return 1;
    if(rssi >= -40.0f)  return 5;
    return (uint8_t)(1 + (rssi + 100.0f) / 15.0f);
}

static uint8_t rssi_to_bar_height(float rssi, uint8_t bar_index) {
    // Each bar lights up progressively; bar_index 0=leftmost (weakest)
    uint8_t filled = rssi_to_bars(rssi);
    if(bar_index >= filled) return 4; // empty bar — just a stub
    // Height grows with bar index
    uint8_t min_h = 6;
    uint8_t step  = (BAR_MAX_H - min_h) / (BAR_COUNT - 1);
    return min_h + step * bar_index;
}

// ─────────────────────────────────────────────────────────────────────────────
// Sparkline — tiny RSSI history at the bottom
// ─────────────────────────────────────────────────────────────────────────────

static void draw_sparkline(Canvas* c, const ScanViewModel* m) {
    // Draw the last RSSI_HISTORY_LEN readings as a tiny line graph
    // across the full width, 8px tall, at y=55
    if(m->history_count < 2) return;
    const uint8_t SPARK_Y = 55;
    const uint8_t SPARK_H = 8;
    const uint8_t SPARK_X = 0;
    const uint8_t SPARK_W = 128;

    uint8_t count = m->history_count < RSSI_HISTORY_LEN ? m->history_count : RSSI_HISTORY_LEN;
    float step_x  = (float)SPARK_W / (float)(count - 1);

    for(uint8_t i = 1; i < count; i++) {
        float r0 = m->history[i - 1];
        float r1 = m->history[i];
        // Normalise -100..-40 → 0..SPARK_H
        uint8_t y0 = SPARK_Y - (uint8_t)((r0 + 100.0f) / 60.0f * SPARK_H);
        uint8_t y1 = SPARK_Y - (uint8_t)((r1 + 100.0f) / 60.0f * SPARK_H);
        uint8_t x0 = SPARK_X + (uint8_t)((i - 1) * step_x);
        uint8_t x1 = SPARK_X + (uint8_t)(i * step_x);
        canvas_draw_line(c, x0, y0, x1, y1);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Draw callback — runs every frame
// ─────────────────────────────────────────────────────────────────────────────

static void scanning_draw_cb(Canvas* canvas, void* model) {
    ScanViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // ── Header bar ────────────────────────────────────────────────────────────
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 8, "RF ROSETTA");

    // Antenna indicator: [INT] or [EXT]
    canvas_draw_str(canvas, 80, 8, m->antenna_external ? "[EXT]" : "[INT]");

    // Mode indicator
    const char* mode_str = "SGHz";
    if(m->mode == ScanModeRFNarrow) mode_str = "NRW";
    if(m->mode == ScanModeRFWide)   mode_str = "WDE";
    canvas_draw_str(canvas, 108, 8, mode_str);

    // Divider
    canvas_draw_line(canvas, 0, 10, 128, 10);

    // ── Signal bars ───────────────────────────────────────────────────────────
    uint8_t filled = rssi_to_bars(m->rssi);
    for(uint8_t i = 0; i < BAR_COUNT; i++) {
        uint8_t h = rssi_to_bar_height(m->rssi, i);
        uint8_t x = BAR_START_X + i * (BAR_W + BAR_GAP);
        uint8_t y = BAR_BASE_Y - h;
        if(i < filled) {
            // Filled bar
            canvas_draw_box(canvas, x, y, BAR_W, h);
        } else {
            // Empty bar stub
            canvas_draw_box(canvas, x, BAR_BASE_Y - 4, BAR_W, 4);
        }
    }

    // ── Listening animation (left of bars) ────────────────────────────────────
    // Simple pulsing dot when idle
    if(!m->signal_detected && !m->analyzing) {
        uint8_t pulse_size = (m->anim_tick / 4) % 4;
        canvas_draw_disc(canvas, 12, 36, pulse_size + 2);
    }

    // ── Frequency and RSSI ────────────────────────────────────────────────────
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, BAR_BASE_Y + 4, m->freq_str);

    char rssi_str[12];
    snprintf(rssi_str, sizeof(rssi_str), "%.0fdBm", (double)m->rssi);
    canvas_draw_str(canvas, 90, BAR_BASE_Y + 4, rssi_str);

    // ── Status text ───────────────────────────────────────────────────────────
    canvas_draw_str(canvas, 0, 64, m->status_str);

    // ── Sparkline ─────────────────────────────────────────────────────────────
    // Only draw when we have enough data and not in result state
    if(!m->signal_detected && m->history_count > 4) {
        draw_sparkline(canvas, m);
    }

    // ── Signal detected overlay ───────────────────────────────────────────────
    if(m->signal_detected && !m->analyzing) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 40, "SIGNAL CAUGHT!");
    }
    if(m->analyzing) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 40, "ANALYSING...");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Input callback — Back exits; OK locks the display freq or scans again
// ─────────────────────────────────────────────────────────────────────────────

static bool scanning_input_cb(InputEvent* ev, void* ctx) {
    RFRosettaApp* app = ctx;
    if(ev->type == InputTypeShort) {
        if(ev->key == InputKeyBack) {
            view_dispatcher_send_custom_event(app->view_dispatcher, RFRosettaEventBackPressed);
            return true;
        }
    }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Timer callback — fires every 100ms from the FuriTimer
// ─────────────────────────────────────────────────────────────────────────────

static void scan_timer_cb(void* ctx) {
    RFRosettaApp* app = ctx;

    furi_mutex_acquire(app->data_mutex, FuriWaitForever);

    float rssi = signal_capture_poll_rssi(app->capture_ctx);
    rssi_history_push(&app->rssi_history, rssi);
    app->current_rssi = rssi;

    // Advance sweep frequency every 3 ticks if not locked on a signal
    static uint8_t sweep_ticks = 0;
    if(!app->signal_detected) {
        sweep_ticks++;
        if(sweep_ticks >= 3) {
            sweep_ticks = 0;
            signal_capture_next_freq(app->capture_ctx);
        }
    }

    bool trigger = (!app->signal_detected && rssi > app->rssi_threshold);

    furi_mutex_release(app->data_mutex);

    if(trigger) {
        // Notify scene manager — will call signal_capture_acquire
        view_dispatcher_send_custom_event(app->view_dispatcher, RFRosettaEventSignalCaught);
    }

    // Update view model
    with_view_model(
        app->scanning_view, ScanViewModel*, vm,
        {
            vm->rssi           = rssi;
            vm->frequency      = signal_capture_current_freq(app->capture_ctx);
            vm->signal_detected = app->signal_detected;
            vm->analyzing       = app->analyzing;
            vm->anim_tick++;

            // Copy history
            uint8_t cnt = app->rssi_history.count < RSSI_HISTORY_LEN
                            ? app->rssi_history.count
                            : RSSI_HISTORY_LEN;
            vm->history_count = cnt;
            for(uint8_t i = 0; i < cnt; i++) {
                vm->history[i] = rssi_history_get(&app->rssi_history, i);
            }

            snprintf(vm->freq_str, sizeof(vm->freq_str), "%lu MHz",
                     (unsigned long)(vm->frequency / 1000000));

            if(app->signal_detected) {
                snprintf(vm->status_str, sizeof(vm->status_str), "Press OK to identify");
            } else {
                snprintf(vm->status_str, sizeof(vm->status_str), "Listening...");
            }
        },
        true); // redraw
}

// ─────────────────────────────────────────────────────────────────────────────
// Scene lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void rf_rosetta_scene_scanning_on_enter(void* ctx) {
    RFRosettaApp* app = ctx;

    // Reset state
    app->signal_detected = false;
    app->analyzing       = false;
    memset(&app->capture, 0, sizeof(app->capture));
    memset(&app->match,   0, sizeof(app->match));

    // Configure the custom view
    view_set_draw_callback(app->scanning_view, scanning_draw_cb);
    view_set_input_callback(app->scanning_view, scanning_input_cb);
    view_set_context(app->scanning_view, app);
    view_allocate_model(app->scanning_view, ViewModelTypeLockFree, sizeof(ScanViewModel));

    // Initialise model
    with_view_model(
        app->scanning_view, ScanViewModel*, vm,
        {
            memset(vm, 0, sizeof(ScanViewModel));
            vm->mode             = app->scan_mode;
            vm->antenna_external = (app->antenna == AntennaExternal);
            vm->rssi             = -100.0f;
            snprintf(vm->status_str, sizeof(vm->status_str), "Starting...");
        },
        false);

    // Start the radio
    signal_capture_set_mode(app->capture_ctx, app->scan_mode);
    signal_capture_set_antenna(app->capture_ctx, app->antenna);
    signal_capture_set_threshold(app->capture_ctx, app->rssi_threshold);
    signal_capture_start(app->capture_ctx);

    // Measure initial noise floor
    app->noise_floor = signal_capture_noise_floor(app->capture_ctx);

    // Start polling timer
    app->scan_timer = furi_timer_alloc(scan_timer_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(app->scan_timer, 100);

    view_dispatcher_switch_to_view(app->view_dispatcher, RFRosettaViewScanning);
}

bool rf_rosetta_scene_scanning_on_event(void* ctx, SceneManagerEvent ev) {
    RFRosettaApp* app = ctx;

    if(ev.type == SceneManagerEventTypeCustom) {
        if(ev.event == RFRosettaEventBackPressed) {
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }

        if(ev.event == RFRosettaEventSignalCaught) {
            furi_mutex_acquire(app->data_mutex, FuriWaitForever);
            app->signal_detected = true;
            app->analyzing       = true;
            furi_mutex_release(app->data_mutex);

            // Capture and analyse
            if(signal_capture_acquire(app->capture_ctx, &app->capture)) {
                app->capture.noise_floor = app->noise_floor;
                app->capture.snr         = app->capture.rssi - app->noise_floor;

                protocol_db_match(&app->capture, &app->match);
                if(app->match.matched) {
                    protocol_db_decode(&app->capture, &app->match);
                }

                rf_rosetta_log_signal(app, &app->capture, &app->match);

                furi_mutex_acquire(app->data_mutex, FuriWaitForever);
                app->analyzing = false;
                furi_mutex_release(app->data_mutex);

                scene_manager_next_scene(app->scene_manager, RFRosettaSceneResult);
            } else {
                // Capture failed — reset and keep scanning
                furi_mutex_acquire(app->data_mutex, FuriWaitForever);
                app->signal_detected = false;
                app->analyzing       = false;
                furi_mutex_release(app->data_mutex);
            }
            return true;
        }
    }
    return false;
}

void rf_rosetta_scene_scanning_on_exit(void* ctx) {
    RFRosettaApp* app = ctx;

    // Stop timer
    if(app->scan_timer) {
        furi_timer_stop(app->scan_timer);
        furi_timer_free(app->scan_timer);
        app->scan_timer = NULL;
    }

    // Stop radio
    signal_capture_stop(app->capture_ctx);
}
