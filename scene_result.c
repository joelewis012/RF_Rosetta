#include "rf_rosetta.h"
#include <gui/elements.h>
#include <stdio.h>
#include <string.h>

// Button indices for the widget
typedef enum {
    BtnDetails = 0,
    BtnSave,
    BtnScanAgain,
} ResultBtn;

static void btn_callback(GuiButtonType type, InputType input_type, void* ctx) {
    RFRosettaApp* app = ctx;
    if(input_type != InputTypeShort) return;
    if(type == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, RFRosettaEventViewDetails);
    } else if(type == GuiButtonTypeLeft) {
        view_dispatcher_send_custom_event(app->view_dispatcher, RFRosettaEventScanAgain);
    }
}

static void save_btn_callback(GuiButtonType type, InputType input_type, void* ctx) {
    RFRosettaApp* app = ctx;
    if(input_type != InputTypeShort) return;
    UNUSED(type);
    view_dispatcher_send_custom_event(app->view_dispatcher, RFRosettaEventSaveSignal);
}

static void export_btn_callback(GuiButtonType type, InputType input_type, void* ctx) {
    RFRosettaApp* app = ctx;
    if(input_type != InputTypeShort) return;
    UNUSED(type);
    rf_rosetta_export_sub(app, &app->capture, &app->match);
    // Show brief confirmation
    widget_reset(app->widget);
    const char* fname = app->last_export_path;
    for(const char* c = app->last_export_path; *c; c++) {
        if(*c == '/') fname = c + 1;
    }
    char conf[52];
    snprintf(conf, sizeof(conf), app->last_export_path[0] ? "%s" : "Export failed", fname);
    widget_add_string_element(app->widget, 64, 22, AlignCenter, AlignTop, FontPrimary,    "Saved as .sub");
    widget_add_string_element(app->widget, 64, 36, AlignCenter, AlignTop, FontSecondary,  conf);
    widget_add_button_element(app->widget, GuiButtonTypeCenter, "OK", btn_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, RFRosettaViewWidget);
}

void rf_rosetta_scene_result_on_enter(void* ctx) {
    RFRosettaApp* app = ctx;
    widget_reset(app->widget);

    ProtocolMatch* m  = &app->match;
    SignalCapture* cap = &app->capture;

    if(m->matched && m->protocol) {
        const ProtocolSignature* p = m->protocol;

        // ── Header ────────────────────────────────────────────────────────────
        char header[32];
        snprintf(header, sizeof(header), "MATCHED  %u%%", m->confidence);
        widget_add_string_element(app->widget, 0, 0, AlignLeft, AlignTop, FontSecondary, header);

        // ── Protocol name ─────────────────────────────────────────────────────
        widget_add_string_multiline_element(app->widget, 0, 10, AlignLeft, AlignTop,
            FontPrimary, p->short_name);

        // ── Category + modulation ─────────────────────────────────────────────
        char cat_mod[40];
        snprintf(cat_mod, sizeof(cat_mod), "%s  %s  %luMHz",
            protocol_category_str(p->category),
            modulation_str(p->modulation),
            (unsigned long)(cap->frequency / 1000000));
        widget_add_string_element(app->widget, 0, 26, AlignLeft, AlignTop, FontSecondary, cat_mod);

        // ── Confidence label ──────────────────────────────────────────────────
        char conf_line[32];
        snprintf(conf_line, sizeof(conf_line), "Confidence: %s", m->conf_label);
        widget_add_string_element(app->widget, 0, 35, AlignLeft, AlignTop, FontSecondary, conf_line);

        // ── Security flag ─────────────────────────────────────────────────────
        if(p->security_concern) {
            widget_add_string_element(app->widget, 0, 44, AlignLeft, AlignTop,
                FontSecondary, "! Security concern — see details");
        } else {
            widget_add_string_element(app->widget, 0, 44, AlignLeft, AlignTop,
                FontSecondary, "No security concerns");
        }

    } else {
        // No match
        widget_add_string_element(app->widget, 0, 0,  AlignLeft, AlignTop, FontSecondary,
            "NO MATCH FOUND");
        widget_add_string_element(app->widget, 0, 12, AlignLeft, AlignTop, FontPrimary,
            "Unknown Signal");

        char freq_line[32];
        snprintf(freq_line, sizeof(freq_line), "%luMHz  %.0fdBm",
            (unsigned long)(cap->frequency / 1000000),
            (double)cap->rssi);
        widget_add_string_element(app->widget, 0, 26, AlignLeft, AlignTop, FontSecondary, freq_line);

        char mod_line[32];
        snprintf(mod_line, sizeof(mod_line), "Mod: %s  BW: %ukHz",
            modulation_str(cap->modulation), cap->bandwidth_khz);
        widget_add_string_element(app->widget, 0, 36, AlignLeft, AlignTop, FontSecondary, mod_line);

        widget_add_string_element(app->widget, 0, 46, AlignLeft, AlignTop, FontSecondary,
            "See details for raw data");
    }

    // ── Buttons ───────────────────────────────────────────────────────────────
    widget_add_button_element(app->widget, GuiButtonTypeLeft,  "Scan",    btn_callback,        app);
    widget_add_button_element(app->widget, GuiButtonTypeRight, "Details", btn_callback,        app);
    widget_add_button_element(app->widget, GuiButtonTypeCenter,"Save",    save_btn_callback,   app);

    view_dispatcher_switch_to_view(app->view_dispatcher, RFRosettaViewWidget);
}

bool rf_rosetta_scene_result_on_event(void* ctx, SceneManagerEvent ev) {
    RFRosettaApp* app = ctx;
    if(ev.type == SceneManagerEventTypeCustom) {
        if(ev.event == RFRosettaEventViewDetails) {
            scene_manager_next_scene(app->scene_manager, RFRosettaSceneDetails);
            return true;
        }
        if(ev.event == RFRosettaEventScanAgain) {
            // Pop back to scanning scene
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
        if(ev.event == RFRosettaEventSaveSignal) {
            if(app->saved_count < MAX_SAVED_SIGNALS) {
                SavedSignal* s = &app->saved[app->saved_count];
                memcpy(&s->capture, &app->capture, sizeof(SignalCapture));
                memcpy(&s->match,   &app->match,   sizeof(ProtocolMatch));
                s->timestamp = app->capture.timestamp;
                // Auto-label
                if(app->match.matched && app->match.protocol) {
                    snprintf(s->label, SAVED_NAME_LEN, "%s", app->match.protocol->short_name);
                } else {
                    snprintf(s->label, SAVED_NAME_LEN, "Unknown %luMHz",
                        (unsigned long)(app->capture.frequency / 1000000));
                }
                app->saved_count++;
                rf_rosetta_save_signals(app);
                notification_message(app->notifications, &sequence_success);
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

void rf_rosetta_scene_result_on_exit(void* ctx) {
    RFRosettaApp* app = ctx;
    widget_reset(app->widget);
}
