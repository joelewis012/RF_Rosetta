#include "rf_rosetta.h"
#include <gui/canvas.h>
#include <gui/elements.h>
#include <string.h>
#include <stdio.h>

// ScanViewModel is defined in rf_rosetta.h so rf_rosetta.c can use it too.

// ─────────────────────────────────────────────────────────────────────────────
// Signal bars helpers
// ─────────────────────────────────────────────────────────────────────────────

// Left-anchored bars, 5 steps, fitted in x=1..54
#define BAR_COUNT    5
#define BAR_W        8
#define BAR_GAP      3
#define BAR_MAX_H   30
#define BAR_BASE_Y  44
#define BAR_START_X  2

static uint8_t rssi_to_bars(float rssi) {
    if(rssi <= -100.0f) return 1;
    if(rssi >= -40.0f)  return 5;
    return (uint8_t)(1.0f + (rssi + 100.0f) / 15.0f);
}

// ─────────────────────────────────────────────────────────────────────────────
// Draw callback
//
// Screen layout (128 × 64 px):
//
//  y=0  ┌─────────────────────────────────────────────────┐
//       │ RF ROSETTA      SubGHz            [INT]         │ ← header
//  y=10 ├─────────────────────────────────────────────────┤
//       │                                                 │
//       │  ▏▎▍▌▋          433 MHz                        │ ← bars + freq
//       │                  -83 dBm                       │ ← rssi
//  y=44 │                                                 │
//  y=46 │ Listening...                                    │ ← status
//  y=55 ├ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┤
//  y=56 │▁▁▂▃▂▁▂▁▁▂▃▂▁                                  │ ← sparkline
//  y=63 └─────────────────────────────────────────────────┘
// ─────────────────────────────────────────────────────────────────────────────

static void scanning_draw_cb(Canvas* canvas, void* model_ptr) {
    ScanViewModel* m = (ScanViewModel*)model_ptr;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // ── Zone 1: Header (y 0–10) ───────────────────────────────────────────────
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 1, 8, "RF ROSETTA");

    // Mode label — 3-char abbreviation keeps it from overlapping app name
    const char* mode_str = "OOK";
    if(m->mode == ScanModeRFNarrow) mode_str = "FSK";
    if(m->mode == ScanModeRFWide)   mode_str = "WID";
    canvas_draw_str(canvas, 68, 8, mode_str);

    // Antenna badge — right-aligned, inverted+warning when external missing
    if(m->antenna_external) {
        if(m->ext_not_found) {
            // EXT requested but cc1101_ext not found — draw with dotted outline
            canvas_draw_frame(canvas, 100, 0, 28, 10);
            canvas_draw_str(canvas, 102, 8, "!EXT");
        } else {
            // EXT confirmed active — solid inverted badge
            canvas_draw_box(canvas, 100, 0, 28, 10);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, 102, 8, "[EXT]");
            canvas_set_color(canvas, ColorBlack);
        }
    } else {
        canvas_draw_str(canvas, 102, 8, "[INT]");
    }

    canvas_draw_line(canvas, 0, 10, 128, 10);

    // ── Zone 2: Main body (y 12–44) ──────────────────────────────────────────

    if(m->signal_detected || m->analyzing) {
        // ── Signal detected / analysing overlay (replaces bars) ──────────────
        canvas_set_font(canvas, FontPrimary);
        if(m->analyzing) {
            canvas_draw_str(canvas, 2, 28, "ANALYSING...");
        } else {
            canvas_draw_str(canvas, 2, 28, "SIGNAL CAUGHT!");
        }
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 40, "Press OK to identify");

    } else {
        // ── Signal bars (left column x 2–53) ─────────────────────────────────
        uint8_t filled = rssi_to_bars(m->rssi);
        uint8_t min_h  = 5;
        uint8_t h_step = (BAR_MAX_H - min_h) / (BAR_COUNT - 1);
        for(uint8_t i = 0; i < BAR_COUNT; i++) {
            uint8_t h = min_h + h_step * i;
            uint8_t x = BAR_START_X + i * (BAR_W + BAR_GAP);
            if(i < filled) {
                canvas_draw_box(canvas, x, BAR_BASE_Y - h, BAR_W, h);
            } else {
                // Empty bar — just an outline
                canvas_draw_frame(canvas, x, BAR_BASE_Y - 4, BAR_W, 4);
            }
        }

        // ── Frequency (right column, large font) ─────────────────────────────
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 60, 26, m->freq_str);

        // ── RSSI + trend arrow (right column, small font) ────────────────────
        char rssi_str[12];
        snprintf(rssi_str, sizeof(rssi_str), "%.0f dBm", (double)m->rssi);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 62, 38, rssi_str);

        // Trend arrow — drawn just to the right of the RSSI value
        if(m->rssi_trend > 0) {
            // Upward triangle (stronger)
            canvas_draw_triangle(canvas, 118, 32, 4, 5, CanvasDirectionTopToBottom);
        } else if(m->rssi_trend < 0) {
            // Downward triangle (weaker)
            canvas_draw_triangle(canvas, 118, 37, 4, 5, CanvasDirectionBottomToTop);
        } else {
            // Steady — small dash
            canvas_draw_line(canvas, 115, 35, 121, 35);
        }

        // ── Fingerprint badge ─────────────────────────────────────────────────
        if(m->seen_before && m->seen_count > 1) {
            char fp_str[8];
            snprintf(fp_str, sizeof(fp_str), "x%d", m->seen_count);
            canvas_draw_frame(canvas, 60, 40, 24, 9);
            canvas_draw_str(canvas, 62, 48, fp_str);
        }

        // ── Idle animation dot (top-left, above bars) ────────────────────────
        uint8_t ps = (m->anim_tick / 8) % 3;
        canvas_draw_disc(canvas, 8, 17, ps + 1);
    }

    // ── Zone 3: Status text (y 46–54) ────────────────────────────────────────
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 1, 54, m->status_str);

    // ── Zone 4: Sparkline strip (y 56–63) ────────────────────────────────────
    // Separate from status text — 8 px strip at very bottom
    if(m->history_count > 1) {
        uint8_t count  = m->history_count;
        float   x_step = 128.0f / (float)(count > 1 ? count - 1 : 1);
        for(uint8_t i = 1; i < count; i++) {
            float r0 = m->history[i - 1];
            float r1 = m->history[i];
            // Map dBm range -120..-40 → pixel row 63..56
            uint8_t y0 = 63 - (uint8_t)((r0 + 120.0f) / 80.0f * 7.0f);
            uint8_t y1 = 63 - (uint8_t)((r1 + 120.0f) / 80.0f * 7.0f);
            y0 = (y0 < 56) ? 56 : (y0 > 63 ? 63 : y0);
            y1 = (y1 < 56) ? 56 : (y1 > 63 ? 63 : y1);
            uint8_t x0 = (uint8_t)((i - 1) * x_step);
            uint8_t x1 = (uint8_t)(i * x_step);
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
        // RSSI trend — compare last 5 samples
        int8_t trend = 0;
        if(app->rssi_history.count >= 5) {
            float recent = rssi_history_get(&app->rssi_history,
                               (uint8_t)(app->rssi_history.count - 1));
            float older  = rssi_history_get(&app->rssi_history,
                               (uint8_t)(app->rssi_history.count > 5 ?
                               app->rssi_history.count - 5 : 0));
            if(recent - older >  3.0f) trend =  1;
            if(older - recent >  3.0f) trend = -1;
        }

        vm->rssi            = rssi;
        vm->rssi_trend      = trend;
        vm->frequency       = signal_capture_current_freq(app->capture_ctx);
        vm->signal_detected = app->signal_detected;
        vm->analyzing       = app->analyzing;
        vm->anim_tick++;
        vm->antenna_external = (app->antenna == AntennaExternal);
        vm->ext_not_found    = !signal_capture_antenna_ok(app->capture_ctx);
        vm->mode             = app->scan_mode;

        // Fingerprint — check session seen count for current signal
        // (populated in the acquire path; just pass through here)

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
        } else if(vm->ext_not_found && app->antenna == AntennaExternal) {
            snprintf(vm->status_str, sizeof(vm->status_str), "EXT not found, using INT");
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

                // ── Signal fingerprinting ─────────────────────────────────
                // Simple hash: freq/MHz bucket + modulation + pulse_avg bucket
                uint32_t fp = (uint32_t)(app->capture.frequency / 1000000) * 1000
                            + (uint32_t)app->capture.modulation * 100
                            + (uint32_t)(app->capture.pulse_avg / 50);
                uint8_t  fp_idx = 0;
                bool     fp_found = false;
                for(uint8_t i = 0; i < app->fingerprint_num; i++) {
                    if(app->fingerprints[i] == fp) {
                        app->fingerprint_counts[i]++;
                        fp_idx = i;
                        fp_found = true;
                        break;
                    }
                }
                if(!fp_found && app->fingerprint_num < 64) {
                    fp_idx = app->fingerprint_num;
                    app->fingerprints[fp_idx] = fp;
                    app->fingerprint_counts[fp_idx] = 1;
                    app->fingerprint_num++;
                }

                // Push fingerprint data into view model
                ScanViewModel* vm = (ScanViewModel*)view_get_model(app->scanning_view);
                if(vm) {
                    vm->seen_before = fp_found;
                    vm->seen_count  = fp_found ? app->fingerprint_counts[fp_idx] : 1;
                }
                view_commit_model(app->scanning_view, false);

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
