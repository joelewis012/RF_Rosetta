#include "rf_rosetta.h"
#include <gui/modules/widget.h>

// ─────────────────────────────────────────────────────────────────────────────
// About text — displayed in a scrollable TextBox
// ─────────────────────────────────────────────────────────────────────────────

static const char ABOUT_TEXT[] =
    "RF ROSETTA v1.0\n"
    "by Joe\n"
    "──────────────\n"
    "\n"
    "SCAN MODES\n"
    "\n"
    "SubGHz (OOK)\n"
    " Best for: remotes,\n"
    " doorbells, sensors.\n"
    " OOK = On-Off Keying.\n"
    " Standard 650kHz BW.\n"
    "\n"
    "Narrow (FSK)\n"
    " Best for: TPMS,\n"
    " utility meters,\n"
    " weather stations.\n"
    " 238kHz deviation.\n"
    " More sensitive,\n"
    " less noise.\n"
    "\n"
    "Wide (FSK)\n"
    " Best for: industrial,\n"
    " pagers, 2-FSK data.\n"
    " 476kHz deviation.\n"
    " Catches more types,\n"
    " noisier floor.\n"
    "\n"
    "ANTENNA\n"
    "\n"
    "Internal: built-in\n"
    " Flipper antenna.\n"
    "\n"
    "External: uses the\n"
    " CC1101 on your dev\n"
    " board (cc1101_ext).\n"
    " Board must be plugged\n"
    " in to take effect.\n"
    " If not found, falls\n"
    " back to internal.\n"
    "\n"
    "NOTE: 2.4 GHz (WiFi,\n"
    " BLE) is NOT supported\n"
    " by the CC1101 chip.\n"
    " CC1101 range:\n"
    " 281-481 MHz +\n"
    " 749-962 MHz.\n"
    "\n"
    "FEATURES\n"
    "\n"
    "RSSI trend arrow:\n"
    " Up arrow = signal\n"
    " getting stronger.\n"
    " Down = weaker.\n"
    " Use to find source.\n"
    "\n"
    "Fingerprint badge:\n"
    " [x2] means this exact\n"
    " signal profile seen\n"
    " twice this session.\n"
    " Same device!\n"
    "\n"
    "──────────────\n"
    "github.com/joelewis012\n"
    "Support: Ko-fi/BMC\n"
    " joelewis012\n";

// ─────────────────────────────────────────────────────────────────────────────
// Scene handlers
// ─────────────────────────────────────────────────────────────────────────────

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
