#include "rf_rosetta.h"
#include <gui/canvas.h>
#include <gui/elements.h>
#include <string.h>
#include <stdio.h>

// ─────────────────────────────────────────────────────────────────────────────
// View model
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    float    rssi;
    float    history[RSSI_HISTORY_LEN];
    uint8_t  history_count;
    uint32_t frequency;
    bool     signal_detected;
    bool     analyzing;
    ScanMode mode;
    bool     antenna_external;
    char     freq_str[20];
    char     status_str[32];
    uint8_t  anim_tick;
} ScanViewModel;

// ─────────────────────────────────────────────────────────────────────────────
// Signal bars helpers
// ─────────────────────────────────────────────────────────────────────────────

#define BAR_COUNT   5
#define BAR_W       10
#define BAR_GAP     4
#define BAR_MAX_H   30
#define BAR_BASE_Y  50
#define BAR_START_X 28

static uint8_t rssi_to_bars(float rssi) {
    if(rssi <= -100.0f) return 1;
    if(rssi >= -40.0f)  return 5;
    return (uint8_t)(1.0f + (rssi + 100.0f) / 15.0f);
}

// ─────────────────────────────────────────────────────────────────────────────
// Draw callback
// ─────────────────────────────────────────────────────────────────────────────

static void scanning_draw_cb(Canvas* canvas, void* model_ptr) {
    ScanViewModel* m = (ScanViewModel*)model_ptr;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Header
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 8, "RF ROSETTA");

    // Antenna indicator — inverted badge when external so it's hard to miss
    if(m->antenna_external) {
        canvas_draw_box(canvas, 75, 0, 27, 10);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, 77, 8, "[EXT]");
        canvas_set_color(canvas, ColorBlack);
    } else {
        canvas_draw_str(canvas, 77, 8, "[INT]");
    }

    const char* mode_str = "SGHz";
    if(m->mode == ScanModeRFNarrow) mode_str = "NRW";
    if(m->mode == ScanModeRFWide)   mode_str = "WDE";
    canvas_draw_str(canvas, 108, 8, mode_str);
    canvas_draw_line(canvas, 0, 10, 128, 10);

    // Signal bars
    uint8_t filled = rssi_to_bars(m->rssi);
    uint8_t min_h  = 6;
    uint8_t step   = (BAR_MAX_H - min_h) / (BAR_COUNT - 1);
    for(uint8_t i = 0; i < BAR_COUNT; i++) {
        uint8_t h = min_h + step * i;
        uint8_t x = BAR_START_X + i * (BAR_W + BAR_GAP);
        if(i < filled) {
            canvas_draw_box(canvas, x, BAR_BASE_Y - h, BAR_W, h);
        } else {
            canvas_draw_box(canvas, x, BAR_BASE_Y - 4, BAR_W, 4);
        }
    }

    // Pulsing dot when idle
    if(!m->signal_detected && !m->analyzing) {
        uint8_t ps = (m->anim_tick / 4) % 4;
        canvas_draw_disc(canvas, 12, 36, ps + 2);
    }

    // Frequency and RSSI
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, BAR_BASE_Y + 4, m->freq_str);

    char rssi_str[12];
    snprintf(rssi_str, sizeof(rssi_str), "%.0fdBm", (double)m->rssi);
    canvas_draw_str(canvas, 90, BAR_BASE_Y + 4, rssi_str);

    // Status
    canvas_draw_str(canvas, 0, 63, m->status_str);

    // Overlay when signal caught
    if(m->signal_detected && !m->analyzing) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 40, "SIGNAL CAUGHT!");
    }
    if(m->analyzing) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 40, "ANALYSING...");
    }

    // Tiny sparkline at bottom
    if(!m->signal_detected && m->history_count > 1) {
        uint8_t count = m->history_count;
        float   step_x = 128.0f / (float)(count > 1 ? count - 1 : 1);
        for(uint8_t i = 1; i < count; i++) {
            float   r0 = m->history[i - 1];
            float   r1 = m->history[i];
            uint8_t y0 = 63 - (uint8_t)((r0 + 100.0f) / 60.0f * 7.0f);
            uint8_t y1 = 63 - (uint8_t)((r1 + 100.0f) / 60.0f * 7.0f);
            uint8_t x0 = (uint8_t)((i - 1) * step_x);
            uint8_t x1 = (uint8_t)(i * step_x);
            canvas_draw_line(canvas, x0, y0, x1, y1);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Input callback
// ─────────────────────────────────────────────────────────────────────────────

static bool scanning_input_cb(InputEvent* ev, void* ctx) {
    RFRosettaApp* app = (RFRosettaApp*)ctx;
    if(ev->type == InputTypeShort && ev->key == InputKeyBack) {
        view_dispatcher_send_custom_event(app->view_dispatcher, RFRosettaEventBackPressed);
        return true;
    }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Timer — fires every 100ms, updates model via direct view_get_model access
// ─────────────────────────────────────────────────────────────────────────────

static void scan_timer_cb(void* ctx) {
    RFRosettaApp* app = (RFRosettaApp*)ctx;

    furi_mutex_acquire(app->data_mutex, FuriWaitForever);

    float rssi = signal_capture_poll_rssi(app->capture_ctx);
    rssi_history_push(&app->rssi_history, rssi);
    app->current_rssi = rssi;

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
        view_dispatcher_send_custom_event(app->view_dispatcher, RFRosettaEventSignalCaught);
    }

    // Update view model — direct access avoids with_view_model macro issues
    ScanViewModel* vm = (ScanViewModel*)view_get_model(app->scanning_view);
    if(vm) {
        vm->rssi            = rssi;
        vm->frequency       = signal_capture_current_freq(app->capture_ctx);
        vm->signal_detected = app->signal_detected;
        vm->analyzing       = app->analyzing;
        vm->anim_tick++;

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
    }
    view_commit_model(app->scanning_view, true);
}

// ─────────────────────────────────────────────────────────────────────────────
// Scene lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void rf_rosetta_scene_scanning_on_enter(void* ctx) {
    RFRosettaApp* app = (RFRosettaApp*)ctx;

    app->signal_detected = false;
    app->analyzing       = false;
    memset(&app->capture, 0, sizeof(app->capture));
    memset(&app->match,   0, sizeof(app->match));

    view_set_draw_callback(app->scanning_view, scanning_draw_cb);
    view_set_input_callback(app->scanning_view, scanning_input_cb);
    view_set_context(app->scanning_view, app);
    // Model is allocated once at app startup — just reset it here

    // Initialise model — direct access
    ScanViewModel* vm = (ScanViewModel*)view_get_model(app->scanning_view);
    if(vm) {
        memset(vm, 0, sizeof(ScanViewModel));
        vm->mode             = app->scan_mode;
        vm->antenna_external = (app->antenna == AntennaExternal);
        vm->rssi             = -100.0f;
        snprintf(vm->status_str, sizeof(vm->status_str), "Starting...");
    }
    view_commit_model(app->scanning_view, false);

    signal_capture_set_mode(app->capture_ctx, app->scan_mode);
    signal_capture_set_antenna(app->capture_ctx, app->antenna);
    signal_capture_set_threshold(app->capture_ctx, app->rssi_threshold);
    signal_capture_start(app->capture_ctx);

    app->noise_floor = signal_capture_noise_floor(app->capture_ctx);

    app->scan_timer = furi_timer_alloc(scan_timer_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(app->scan_timer, 100);

    view_dispatcher_switch_to_view(app->view_dispatcher, RFRosettaViewScanning);
}

bool rf_rosetta_scene_scanning_on_event(void* ctx, SceneManagerEvent ev) {
    RFRosettaApp* app = (RFRosettaApp*)ctx;

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
    RFRosettaApp* app = (RFRosettaApp*)ctx;
    if(app->scan_timer) {
        furi_timer_stop(app->scan_timer);
        furi_timer_free(app->scan_timer);
        app->scan_timer = NULL;
    }
    signal_capture_stop(app->capture_ctx);
}
