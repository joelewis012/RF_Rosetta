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

    // Header
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 1, 8, "NRF24  2.4GHz");
    char sw_str[12];
    snprintf(sw_str, sizeof(sw_str), "#%lu", (unsigned long)m->sweep_count);
    canvas_draw_str(canvas, 100, 8, sw_str);
    canvas_draw_line(canvas, 0, 10, 128, 10);

    if(m->pkt_valid) {
        // ── Packet capture display ────────────────────────────────────────────
        char freq_str[24];
        uint32_t mhz = m->pkt_freq_khz / 1000;
        uint32_t rem = m->pkt_freq_khz % 1000;
        snprintf(freq_str, sizeof(freq_str), "Ch%d  %lu.%03luGHz",
                 m->pkt_channel, (unsigned long)mhz, (unsigned long)rem);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 1, 22, freq_str);
        canvas_draw_str(canvas, 1, 33, "Bytes:");
        canvas_draw_str(canvas, 1, 44, m->pkt_str);
        canvas_draw_str(canvas, 1, 55, "OK=clear  Back=menu");
    } else {
        // ── Channel graph ─────────────────────────────────────────────────────
        uint8_t graph_base = 50;
        uint8_t graph_h    = 36;
        uint8_t scale = m->max_hits > 0 ? m->max_hits : 1;

        for(uint8_t ch = 0; ch < NRF24_CHANNELS; ch++) {
            if(m->hits[ch] > 0) {
                uint8_t h = (uint8_t)((uint16_t)m->hits[ch] * graph_h / scale);
                if(h < 2) h = 2;
                canvas_draw_line(canvas, 2 + ch, graph_base - h, 2 + ch, graph_base);
            }
        }
        canvas_draw_line(canvas, 2, graph_base, 127, graph_base);

        // WiFi ch markers (3px tick below baseline)
        uint8_t wifi_chs[] = {13, 38, 63};
        for(uint8_t i = 0; i < 3; i++) {
            canvas_draw_line(canvas, 2+wifi_chs[i], graph_base+1, 2+wifi_chs[i], graph_base+3);
        }
        // BLE advertising ch markers (2px tick)
        uint8_t ble_chs[] = {1, 25, 79};
        for(uint8_t i = 0; i < 3; i++) {
            canvas_draw_line(canvas, 2+ble_chs[i], graph_base+1, 2+ble_chs[i], graph_base+2);
        }

        canvas_draw_str(canvas, 2,   63, "2.4");
        canvas_draw_str(canvas, 50,  63, "2.45");
        canvas_draw_str(canvas, 100, 63, "2.5G");

        if(m->sweep_count == 0) {
            canvas_draw_str(canvas, 2, 35, "Scanning...");
        } else {
            canvas_draw_str(canvas, 1, 59, "OK=capture pkt");
        }
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
    nrf24_scanner_sweep(&app->nrf24_result);

    NRF24ViewModel* vm = (NRF24ViewModel*)view_get_model(app->nrf24_view);
    if(vm) {
        memcpy(vm->hits, app->nrf24_result.hits, NRF24_CHANNELS);
        vm->max_hits    = app->nrf24_result.max_hits;
        vm->sweep_count = app->nrf24_result.sweep_count;

        // Copy latest packet if valid
        if(app->nrf24_last_pkt.valid) {
            vm->pkt_valid    = true;
            vm->pkt_channel  = app->nrf24_last_pkt.channel;
            vm->pkt_freq_khz = app->nrf24_last_pkt.freq_khz;
            vm->pkt_len      = app->nrf24_last_pkt.length > 8 ? 8 : app->nrf24_last_pkt.length;
            memcpy(vm->pkt_data, app->nrf24_last_pkt.payload, vm->pkt_len);

            // Build hex string: "A1 B2 C3 D4 E5 F6 07 08"
            char* p = vm->pkt_str;
            for(uint8_t i = 0; i < vm->pkt_len; i++) {
                uint8_t b = vm->pkt_data[i];
                *p++ = "0123456789ABCDEF"[b >> 4];
                *p++ = "0123456789ABCDEF"[b & 0xF];
                *p++ = ' ';
            }
            if(p > vm->pkt_str) *(p-1) = '\0';
            else *p = '\0';
        } else {
            vm->pkt_valid = false;
        }
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
            // If packet showing → clear it; otherwise reset scan counts
            if(app->nrf24_last_pkt.valid) {
                app->nrf24_last_pkt.valid = false;
                NRF24ViewModel* vm = (NRF24ViewModel*)view_get_model(app->nrf24_view);
                if(vm) vm->pkt_valid = false;
                view_commit_model(app->nrf24_view, true);
            } else {
                // OK pressed on scan graph — attempt packet capture on busiest channel
                if(app->nrf24_timer) {
                    furi_timer_stop(app->nrf24_timer);
                }
                // Find most active channel
                uint8_t best_ch = 0;
                for(uint8_t ch = 1; ch < NRF24_CHANNELS; ch++) {
                    if(app->nrf24_result.hits[ch] > app->nrf24_result.hits[best_ch]) {
                        best_ch = ch;
                    }
                }
                // Try to capture (300ms window)
                nrf24_capture_packet(best_ch, 300, &app->nrf24_last_pkt);

                if(app->nrf24_timer) {
                    furi_timer_start(app->nrf24_timer, 50);
                }
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
