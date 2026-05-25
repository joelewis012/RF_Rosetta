#include "rf_rosetta.h"
#include <string.h>

// ─────────────────────────────────────────────────────────────────────────────
// Scan Mode setting
// ─────────────────────────────────────────────────────────────────────────────

static const char* SCAN_MODE_LABELS[] = {"Sub-GHz", "RF Narrow", "RF Wide"};
static const uint8_t SCAN_MODE_COUNT = 3;

static void scan_mode_change(VariableItem* item) {
    RFRosettaApp* app    = variable_item_get_context(item);
    uint8_t        index = variable_item_get_current_value_index(item);
    app->scan_mode       = (ScanMode)index;
    variable_item_set_current_value_text(item, SCAN_MODE_LABELS[index]);
}

// ─────────────────────────────────────────────────────────────────────────────
// Antenna setting
// ─────────────────────────────────────────────────────────────────────────────

static const char* ANTENNA_LABELS[] = {"Internal", "External"};
static const uint8_t ANTENNA_COUNT  = 2;

static void antenna_change(VariableItem* item) {
    RFRosettaApp* app    = variable_item_get_context(item);
    uint8_t        index = variable_item_get_current_value_index(item);
    app->antenna         = (AntennaMode)index;
    variable_item_set_current_value_text(item, ANTENNA_LABELS[index]);
    signal_capture_set_antenna(app->capture_ctx, app->antenna);
}

// ─────────────────────────────────────────────────────────────────────────────
// RSSI threshold setting
// ─────────────────────────────────────────────────────────────────────────────

static const int8_t THRESHOLD_VALUES[]  = {-90, -85, -80, -75, -70, -65, -60};
static const char* THRESHOLD_LABELS[]   = {"-90", "-85", "-80", "-75", "-70", "-65", "-60"};
static const uint8_t THRESHOLD_COUNT    = 7;

static void threshold_change(VariableItem* item) {
    RFRosettaApp* app    = variable_item_get_context(item);
    uint8_t        index = variable_item_get_current_value_index(item);
    app->rssi_threshold  = (float)THRESHOLD_VALUES[index];
    variable_item_set_current_value_text(item, THRESHOLD_LABELS[index]);
    signal_capture_set_threshold(app->capture_ctx, app->rssi_threshold);
}

// ─────────────────────────────────────────────────────────────────────────────
// Logging setting
// ─────────────────────────────────────────────────────────────────────────────

static const char* LOG_LABELS[] = {"Off", "On"};

static void logging_change(VariableItem* item) {
    RFRosettaApp* app    = variable_item_get_context(item);
    uint8_t        index = variable_item_get_current_value_index(item);
    app->logging_enabled = (index == 1);
    variable_item_set_current_value_text(item, LOG_LABELS[index]);
}

// ─────────────────────────────────────────────────────────────────────────────
// Scene lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void rf_rosetta_scene_settings_on_enter(void* ctx) {
    RFRosettaApp* app = ctx;
    variable_item_list_reset(app->var_list);

    // Scan Mode
    VariableItem* item;
    item = variable_item_list_add(app->var_list, "Scan Mode",
        SCAN_MODE_COUNT, scan_mode_change, app);
    variable_item_set_current_value_index(item, (uint8_t)app->scan_mode);
    variable_item_set_current_value_text(item, SCAN_MODE_LABELS[(uint8_t)app->scan_mode]);

    // Antenna
    item = variable_item_list_add(app->var_list, "Antenna",
        ANTENNA_COUNT, antenna_change, app);
    variable_item_set_current_value_index(item, (uint8_t)app->antenna);
    variable_item_set_current_value_text(item, ANTENNA_LABELS[(uint8_t)app->antenna]);

    // RSSI Threshold — find closest index
    uint8_t thr_idx = 2; // default -80
    for(uint8_t i = 0; i < THRESHOLD_COUNT; i++) {
        if((float)THRESHOLD_VALUES[i] <= app->rssi_threshold) thr_idx = i;
    }
    item = variable_item_list_add(app->var_list, "Threshold dBm",
        THRESHOLD_COUNT, threshold_change, app);
    variable_item_set_current_value_index(item, thr_idx);
    variable_item_set_current_value_text(item, THRESHOLD_LABELS[thr_idx]);

    // Logging
    item = variable_item_list_add(app->var_list, "SD Logging",
        2, logging_change, app);
    uint8_t log_idx = app->logging_enabled ? 1 : 0;
    variable_item_set_current_value_index(item, log_idx);
    variable_item_set_current_value_text(item, LOG_LABELS[log_idx]);

    view_dispatcher_switch_to_view(app->view_dispatcher, RFRosettaViewVarList);
}

bool rf_rosetta_scene_settings_on_event(void* ctx, SceneManagerEvent ev) {
    UNUSED(ctx);
    UNUSED(ev);
    return false;
}

void rf_rosetta_scene_settings_on_exit(void* ctx) {
    RFRosettaApp* app = ctx;
    variable_item_list_reset(app->var_list);
}
