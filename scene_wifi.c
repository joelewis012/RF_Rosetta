#include "rf_rosetta.h"
#include "esp32_marauder.h"
#include <string.h>

// (WiFiViewModel is defined in rf_rosetta.h so rf_rosetta.c can allocate it)

// ─────────────────────────────────────────────────────────────────────────────
// Draw callback — terminal-style rolling text view
//
//  y=0-10   Header: "WiFi Scan (Marauder)"   scanning indicator
//  y=10     Separator
//  y=12-58  Last 6 lines of raw serial output
//  y=58-63  Status: AP count, OK=start/stop hint
// ─────────────────────────────────────────────────────────────────────────────

static void wifi_draw_cb(Canvas* canvas, void* model_ptr) {
    WiFiViewModel* m = (WiFiViewModel*)model_ptr;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 1, 8, "WiFi Scan (Marauder)");
    canvas_draw_str(canvas, 108, 8, m->scanning ? "LIVE" : "OFF");
    canvas_draw_line(canvas, 0, 10, 128, 10);

    // Show the most recent lines, oldest at top, newest at bottom
    uint8_t show = m->line_count > 6 ? 6 : m->line_count;
    uint8_t start = m->line_count > 6 ? (uint8_t)(m->line_count - 6) : 0;
    for(uint8_t i = 0; i < show; i++) {
        canvas_draw_str(canvas, 1, (uint8_t)(19 + i * 8), m->lines[start + i]);
    }

    if(m->line_count == 0) {
        canvas_draw_str(canvas, 1, 30,
            m->scanning ? "Waiting for data..." : "Press OK to scan");
    }

    canvas_draw_line(canvas, 0, 56, 128, 56);
    char status[32];
    snprintf(status, sizeof(status), "APs: %d", m->ap_count);
    canvas_draw_str(canvas, 1, 63, status);
    canvas_draw_str(canvas, 70, 63, m->scanning ? "OK=stop" : "OK=scan");
}

// ─────────────────────────────────────────────────────────────────────────────
// Input
// ─────────────────────────────────────────────────────────────────────────────

static bool wifi_input_cb(InputEvent* ev, void* ctx) {
    RFRosettaApp* app = ctx;
    if(ev->type != InputTypeShort) return false;
    if(ev->key == InputKeyBack) {
        view_dispatcher_send_custom_event(app->view_dispatcher, RFRosettaEventBackPressed);
        return true;
    }
    if(ev->key == InputKeyOk) {
        view_dispatcher_send_custom_event(app->view_dispatcher, RFRosettaEventWifiToggleScan);
        return true;
    }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Timer — polls the UART bridge and refreshes the view model
// ─────────────────────────────────────────────────────────────────────────────

static void wifi_timer_cb(void* ctx) {
    RFRosettaApp* app = ctx;

    esp32_marauder_poll(app->marauder, &app->wifi_lines, &app->wifi_result);

    WiFiViewModel* vm = (WiFiViewModel*)view_get_model(app->wifi_view);
    if(vm) {
        vm->line_count = app->wifi_lines.count > MARAUDER_LINES_BUF
                            ? MARAUDER_LINES_BUF : app->wifi_lines.count;
        for(uint8_t i = 0; i < vm->line_count; i++) {
            snprintf(vm->lines[i], MARAUDER_LINE_MAX, "%s", app->wifi_lines.lines[i]);
        }
        vm->ap_count  = app->wifi_result.count;
        vm->scanning  = app->wifi_result.scanning;
    }
    view_commit_model(app->wifi_view, true);
}

// ─────────────────────────────────────────────────────────────────────────────
// Scene lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void rf_rosetta_scene_wifi_on_enter(void* ctx) {
    RFRosettaApp* app = ctx;

    memset(&app->wifi_lines, 0, sizeof(app->wifi_lines));
    memset(&app->wifi_result, 0, sizeof(app->wifi_result));

    if(!app->marauder) {
        app->marauder = esp32_marauder_alloc();
    }
    esp32_marauder_init(app->marauder);

    WiFiViewModel* vm = (WiFiViewModel*)view_get_model(app->wifi_view);
    if(vm) memset(vm, 0, sizeof(WiFiViewModel));
    view_commit_model(app->wifi_view, false);

    view_set_draw_callback(app->wifi_view, wifi_draw_cb);
    view_set_input_callback(app->wifi_view, wifi_input_cb);
    view_set_context(app->wifi_view, app);

    app->wifi_timer = furi_timer_alloc(wifi_timer_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(app->wifi_timer, 150);

    view_dispatcher_switch_to_view(app->view_dispatcher, RFRosettaViewWifi);
}

bool rf_rosetta_scene_wifi_on_event(void* ctx, SceneManagerEvent ev) {
    RFRosettaApp* app = ctx;
    if(ev.type == SceneManagerEventTypeCustom) {
        if(ev.event == RFRosettaEventBackPressed) {
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
        if(ev.event == RFRosettaEventWifiToggleScan) {
            if(app->wifi_result.scanning) {
                esp32_marauder_stop_scan(app->marauder);
                app->wifi_result.scanning = false;
            } else {
                esp32_marauder_start_scan(app->marauder);
                app->wifi_result.scanning = true;
            }
            return true;
        }
    }
    if(ev.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

void rf_rosetta_scene_wifi_on_exit(void* ctx) {
    RFRosettaApp* app = ctx;
    if(app->wifi_timer) {
        furi_timer_stop(app->wifi_timer);
        furi_timer_free(app->wifi_timer);
        app->wifi_timer = NULL;
    }
    if(app->marauder) {
        if(app->wifi_result.scanning) esp32_marauder_stop_scan(app->marauder);
        esp32_marauder_deinit(app->marauder);
    }
}
