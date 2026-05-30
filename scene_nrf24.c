#include "rf_rosetta.h"
#include "nrf24_scanner.h"
#include <string.h>

// ─────────────────────────────────────────────────────────────────────────────
// View model
// ─────────────────────────────────────────────────────────────────────────────

#define NRF_GRAPH_BASE 52   // y pixel of bar graph baseline
#define NRF_GRAPH_H    38   // max bar height in pixels

// ─────────────────────────────────────────────────────────────────────────────
// Draw callback
//
// Layout:
//  y=0-10   Header: "NRF24 2.4GHz"  |  sweep count
//  y=10     Separator
//  y=12-52  Channel bar graph (125 channels = 125px wide, 1px per channel)
//  y=52     Baseline
//  y=53-63  Frequency labels + WiFi/BLE channel markers
// ─────────────────────────────────────────────────────────────────────────────

static void nrf24_draw_cb(Canvas* canvas, void* model_ptr) {
    NRF24ViewModel* m = (NRF24ViewModel*)model_ptr;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // ── Header ────────────────────────────────────────────────────────────────
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 1, 8, "NRF24  2.4 GHz");

    char sw_str[16];
    snprintf(sw_str, sizeof(sw_str), "#%lu", (unsigned long)m->sweep_count);
    canvas_draw_str(canvas, 100, 8, sw_str);
    canvas_draw_line(canvas, 0, 10, 128, 10);

    // ── Channel bars ─────────────────────────────────────────────────────────
    uint8_t scale = m->max_hits > 0 ? m->max_hits : 1;
    for(uint8_t ch = 0; ch < NRF24_CHANNELS; ch++) {
        if(m->hits[ch] > 0) {
            uint8_t h = (uint8_t)((uint16_t)m->hits[ch] * NRF_GRAPH_H / scale);
            if(h < 2) h = 2; // minimum visible bar
            uint8_t x = 2 + ch;
            canvas_draw_line(canvas, x, NRF_GRAPH_BASE - h, x, NRF_GRAPH_BASE);
        }
    }

    // Baseline
    canvas_draw_line(canvas, 2, NRF_GRAPH_BASE, 127, NRF_GRAPH_BASE);

    // ── WiFi & BLE channel markers (tick marks below baseline) ───────────────
    // WiFi ch1  = 2.412 GHz = NRF24 ch 11  → x=13
    // WiFi ch6  = 2.437 GHz = NRF24 ch 36  → x=38
    // WiFi ch11 = 2.462 GHz = NRF24 ch 61  → x=63
    // WiFi ch13 = 2.472 GHz = NRF24 ch 71  → x=73
    // BLE adv   = 2.402 GHz = NRF24 ch 1   → x=3  (ch37)
    // BLE adv   = 2.426 GHz = NRF24 ch 25  → x=27 (ch38)
    // BLE adv   = 2.480 GHz = NRF24 ch 79  → x=81 (ch39)

    // WiFi markers — 3px tick
    uint8_t wifi_chs[] = {13, 38, 63};
    for(uint8_t i = 0; i < 3; i++) {
        uint8_t x = 2 + wifi_chs[i];
        canvas_draw_line(canvas, x, NRF_GRAPH_BASE + 1, x, NRF_GRAPH_BASE + 3);
    }

    // BLE advertising markers — 2px tick
    uint8_t ble_chs[] = {3, 27, 81};
    for(uint8_t i = 0; i < 3; i++) {
        uint8_t x = 2 + ble_chs[i];
        canvas_draw_line(canvas, x, NRF_GRAPH_BASE + 1, x, NRF_GRAPH_BASE + 2);
    }

    // ── Frequency labels ─────────────────────────────────────────────────────
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2,   63, "2.4");
    canvas_draw_str(canvas, 50,  63, "2.45");
    canvas_draw_str(canvas, 100, 63, "2.5G");

    // ── Status / instruction ─────────────────────────────────────────────────
    if(m->sweep_count == 0) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 35, "Scanning...");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Input callback
// ─────────────────────────────────────────────────────────────────────────────

static bool nrf24_input_cb(InputEvent* event, void* ctx) {
    RFRosettaApp* app = ctx;
    if(event->type != InputTypeShort) return false;
    if(event->key == InputKeyBack) {
        view_dispatcher_send_custom_event(app->view_dispatcher, RFRosettaEventBackPressed);
        return true;
    }
    if(event->key == InputKeyOk) {
        view_dispatcher_send_custom_event(app->view_dispatcher, RFRosettaEventNRF24Reset);
        return true;
    }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Timer callback — runs sweep on a background-safe timer
// ─────────────────────────────────────────────────────────────────────────────

static void nrf24_timer_cb(void* ctx) {
    RFRosettaApp* app = ctx;

    // Run a sweep (blocking ~25ms — acceptable in timer context)
    nrf24_scanner_sweep(&app->nrf24_result);

    // Copy into view model
    NRF24ViewModel* vm = (NRF24ViewModel*)view_get_model(app->nrf24_view);
    if(vm) {
        memcpy(vm->hits, app->nrf24_result.hits, NRF24_CHANNELS);
        vm->max_hits    = app->nrf24_result.max_hits;
        vm->sweep_count = app->nrf24_result.sweep_count;
    }
    view_commit_model(app->nrf24_view, true);
}

static void nrf24_not_found_cb(GuiButtonType result, InputType type, void* context) {
    UNUSED(result);
    if(type != InputTypeShort) return;
    RFRosettaApp* app = context;
    scene_manager_previous_scene(app->scene_manager);
}

// ─────────────────────────────────────────────────────────────────────────────
// Scene lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void rf_rosetta_scene_nrf24_on_enter(void* ctx) {
    RFRosettaApp* app = ctx;

    // ── Hardware detection ────────────────────────────────────────────────────
    // Init GPIO and power up NRF24, then read back CONFIG register to verify
    // the chip is actually present and the board switch is in NRF24 position.
    nrf24_scanner_init();

    if(!nrf24_is_connected()) {
        // Chip not responding — board not connected or switch in wrong position
        nrf24_scanner_deinit();
        widget_reset(app->widget);
        widget_add_string_element(app->widget, 64, 6,  AlignCenter, AlignTop, FontSecondary, "NRF24 Not Detected");
        widget_add_string_element(app->widget, 64, 18, AlignCenter, AlignTop, FontSecondary, "Connect your 3-in-1");
        widget_add_string_element(app->widget, 64, 29, AlignCenter, AlignTop, FontSecondary, "board and flip the");
        widget_add_string_element(app->widget, 64, 40, AlignCenter, AlignTop, FontSecondary, "switch to NRF24.");
        widget_add_button_element(app->widget, GuiButtonTypeCenter, "OK", nrf24_not_found_cb, app);
        view_dispatcher_switch_to_view(app->view_dispatcher, RFRosettaViewWidget);
        return;
    }

    // ── Chip confirmed — start scanning ──────────────────────────────────────
    nrf24_scanner_reset(&app->nrf24_result);

    // Reset view model
    NRF24ViewModel* vm = (NRF24ViewModel*)view_get_model(app->nrf24_view);
    if(vm) {
        memset(vm, 0, sizeof(NRF24ViewModel));
    }
    view_commit_model(app->nrf24_view, false);

    view_set_draw_callback(app->nrf24_view, nrf24_draw_cb);
    view_set_input_callback(app->nrf24_view, nrf24_input_cb);
    view_set_context(app->nrf24_view, app);

    // Start sweep timer — every 50ms (sweep takes ~25ms, leaving 25ms headroom)
    app->nrf24_timer = furi_timer_alloc(nrf24_timer_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(app->nrf24_timer, 50);

    view_dispatcher_switch_to_view(app->view_dispatcher, RFRosettaViewNRF24);
}

bool rf_rosetta_scene_nrf24_on_event(void* ctx, SceneManagerEvent ev) {
    RFRosettaApp* app = ctx;
    if(ev.type == SceneManagerEventTypeCustom) {
        if(ev.event == RFRosettaEventBackPressed) {
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
        if(ev.event == RFRosettaEventNRF24Reset) {
            nrf24_scanner_reset(&app->nrf24_result);
            NRF24ViewModel* vm = (NRF24ViewModel*)view_get_model(app->nrf24_view);
            if(vm) memset(vm, 0, sizeof(NRF24ViewModel));
            view_commit_model(app->nrf24_view, true);
            return true;
        }
    }
    if(ev.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

void rf_rosetta_scene_nrf24_on_exit(void* ctx) {
    RFRosettaApp* app = ctx;
    if(app->nrf24_timer) {
        furi_timer_stop(app->nrf24_timer);
        furi_timer_free(app->nrf24_timer);
        app->nrf24_timer = NULL;
        nrf24_scanner_deinit();  // only deinit if we actually started
    }
    widget_reset(app->widget);
}
