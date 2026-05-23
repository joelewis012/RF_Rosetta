#include "rf_rosetta.h"
#include <stdio.h>
#include <string.h>

// Build the full details string and push it into the TextBox.
// This is everything we know about the captured signal.

static void build_details(RFRosettaApp* app, char* buf, size_t len) {
    SignalCapture* cap = &app->capture;
    ProtocolMatch* m   = &app->match;
    int pos = 0;

    // ── Signal Measurements ───────────────────────────────────────────────────
    pos += snprintf(buf + pos, len - pos,
        "--- SIGNAL ---\n"
        "Freq:     %lu MHz\n"
        "RSSI:     %.0f dBm\n"
        "Noise:    %.0f dBm\n"
        "SNR:      %.1f dB\n"
        "Mod:      %s\n"
        "BW:       %u kHz\n"
        "Pulse avg: %u us\n"
        "Pulse min: %u us\n"
        "Pulse max: %u us\n"
        "Pulses:   %u captured\n"
        "Bits est: ~%u\n"
        "Repeats:  %u\n"
        "Repeating: %s\n"
        "Fixed code: %s\n\n",
        (unsigned long)(cap->frequency / 1000000),
        (double)cap->rssi,
        (double)cap->noise_floor,
        (double)cap->snr,
        modulation_str(cap->modulation),
        cap->bandwidth_khz,
        cap->pulse_avg,
        cap->pulse_min,
        cap->pulse_max,
        cap->pulse_count,
        cap->packet_bits,
        cap->repeat_count,
        cap->repeating ? "Yes" : "No",
        cap->fixed_code_likely ? "Likely" : "Unlikely");

    if(pos >= (int)len - 10) return;

    // ── Match Result ──────────────────────────────────────────────────────────
    if(m->matched && m->protocol) {
        const ProtocolSignature* p = m->protocol;
        pos += snprintf(buf + pos, len - pos,
            "--- IDENTIFICATION ---\n"
            "Protocol: %s\n"
            "Category: %s\n"
            "Confidence: %u%% (%s)\n"
            "Encrypted: %s\n"
            "Rolling code: %s\n"
            "Needs ESP32: %s\n\n",
            p->name,
            protocol_category_str(p->category),
            m->confidence, m->conf_label,
            p->encrypted     ? "Yes" : "No",
            p->rolling_code  ? "Yes" : "No",
            p->needs_esp32   ? "Yes" : "No");

        if(pos < (int)len - 10 && p->brands) {
            pos += snprintf(buf + pos, len - pos,
                "--- BRANDS ---\n%s\n\n", p->brands);
        }

        if(pos < (int)len - 10 && p->description) {
            pos += snprintf(buf + pos, len - pos,
                "--- ABOUT ---\n%s\n\n", p->description);
        }

        if(pos < (int)len - 10 && p->security_note) {
            pos += snprintf(buf + pos, len - pos,
                "--- SECURITY ---\n%s\n\n", p->security_note);
        }

        if(pos < (int)len - 10 && p->extra_data) {
            pos += snprintf(buf + pos, len - pos,
                "--- DECODABLE DATA ---\n%s\n\n", p->extra_data);
        }

        if(pos < (int)len - 10 && m->decoded[0]) {
            pos += snprintf(buf + pos, len - pos,
                "--- CAPTURE DETAILS ---\n%s\n", m->decoded);
        }
    } else {
        pos += snprintf(buf + pos, len - pos,
            "--- NO MATCH ---\n"
            "Signal did not match any known\n"
            "protocol in the database.\n\n"
            "This could be:\n"
            "- A proprietary protocol\n"
            "- A protocol not yet in DB\n"
            "- Noise or interference\n\n"
            "Raw pulse data captured.\n"
            "Submit to contribute to DB.\n");
    }
}

#define DETAILS_BUF_LEN 1536

void rf_rosetta_scene_details_on_enter(void* ctx) {
    RFRosettaApp* app = ctx;

    char* buf = malloc(DETAILS_BUF_LEN);
    furi_assert(buf);
    memset(buf, 0, DETAILS_BUF_LEN);

    // Check if viewing a saved signal or the live capture
    bool viewing_saved = (app->selected_saved < app->saved_count) &&
                         (app->scene_manager != NULL);

    if(viewing_saved) {
        // Temporarily swap in saved capture/match for display
        SignalCapture orig_cap = app->capture;
        ProtocolMatch orig_match = app->match;
        app->capture = app->saved[app->selected_saved].capture;
        app->match   = app->saved[app->selected_saved].match;
        build_details(app, buf, DETAILS_BUF_LEN);
        app->capture = orig_cap;
        app->match   = orig_match;
    } else {
        build_details(app, buf, DETAILS_BUF_LEN);
    }

    text_box_reset(app->text_box);
    text_box_set_text(app->text_box, buf);
    free(buf);

    view_dispatcher_switch_to_view(app->view_dispatcher, RFRosettaViewTextBox);
}

bool rf_rosetta_scene_details_on_event(void* ctx, SceneManagerEvent ev) {
    UNUSED(ctx);
    if(ev.type == SceneManagerEventTypeBack) return false; // let view dispatcher handle
    return false;
}

void rf_rosetta_scene_details_on_exit(void* ctx) {
    RFRosettaApp* app = ctx;
    text_box_reset(app->text_box);
    // Reset saved-signal flag
    app->selected_saved = 0xFF;
}
