#include "rf_rosetta.h"

static const char ABOUT_TEXT[] =
    "RF ROSETTA v1.0\n"
    "by Joe\n"
    "──────────────\n"
    "\n"
    "3-IN-1 DEV BOARD\n"
    "\n"
    " CC1101 (high gain)\n"
    "  = THIS APP\n"
    "  Great range boost\n"
    "  vs internal ant.\n"
    "\n"
    " ESP32/WiFi 2.4GHz\n"
    "  = WiFi Marauder\n"
    "  (separate app)\n"
    "\n"
    " nRF24 Sniffer\n"
    "  = MouseJack apps\n"
    "  (separate app)\n"
    "\n"
    "SCAN MODES\n"
    "\n"
    "OOK (Sub-GHz)\n"
    " On-Off Keying.\n"
    " Remotes, doorbells,\n"
    " garage doors, PIR.\n"
    " 650 kHz BW.\n"
    " Most common mode.\n"
    "\n"
    "FSK-N (Narrow)\n"
    " 2-FSK 238kHz dev.\n"
    " TPMS tyre sensors,\n"
    " weather stations,\n"
    " utility meters.\n"
    " Higher sensitivity.\n"
    "\n"
    "FSK-W (Wide)\n"
    " 2-FSK 476kHz dev.\n"
    " Industrial sensors,\n"
    " pagers, unknown\n"
    " FSK signal types.\n"
    "\n"
    "RSSI TREND\n"
    " Up   = closer\n"
    " Down = moving away\n"
    " Dash = steady\n"
    " Use to locate TX.\n"
    "\n"
    "FINGERPRINT\n"
    " [x2] badge = same\n"
    " signal profile seen\n"
    " again this session.\n"
    "\n"
    "──────────────\n"
    "Ko-fi / BMC:\n"
    " joelewis012\n";

void rf_rosetta_scene_about_on_enter(void* context) {
    RFRosettaApp* app = context;
    text_box_reset(app->text_box);
    text_box_set_font(app->text_box, TextBoxFontText);
    text_box_set_text(app->text_box, ABOUT_TEXT);
    view_dispatcher_switch_to_view(app->view_dispatcher, RFRosettaViewTextBox);
}

bool rf_rosetta_scene_about_on_event(void* context, SceneManagerEvent ev) {
    UNUSED(context);
    UNUSED(ev);
    return false;
}

void rf_rosetta_scene_about_on_exit(void* context) {
    RFRosettaApp* app = context;
    text_box_reset(app->text_box);
}
