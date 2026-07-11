#include "rf_rosetta.h"
#include <string.h>

// ─────────────────────────────────────────────────────────────────────────────
// Scan Mode setting
// ─────────────────────────────────────────────────────────────────────────────

static const char* SCAN_MODE_LABELS[] = {"All (OOK+FSK)", "OOK only", "FSK Narrow", "FSK Wide"};
static const uint8_t SCAN_MODE_COUNT = 4;

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
// Dwell speed setting
// ─────────────────────────────────────────────────────────────────────────────

static const uint16_t DWELL_VALUES[] = {100, 200, 300, 500, 750, 1000, 2000};
static const char* DWELL_LABELS[]    = {"100ms","200ms","300ms","500ms","750ms","1s","2s"};
static const uint8_t DWELL_COUNT     = 7;

static void dwell_change(VariableItem* item) {
    RFRosettaApp* app    = variable_item_get_context(item);
    uint8_t        index = variable_item_get_current_value_index(item);
    signal_capture_set_dwell(app->capture_ctx, DWELL_VALUES[index]);
    variable_item_set_current_value_text(item, DWELL_LABELS[index]);
}

// ─────────────────────────────────────────────────────────────────────────────
// Board / GPIO preset setting
// ─────────────────────────────────────────────────────────────────────────────

static void board_preset_change(VariableItem* item) {
    RFRosettaApp* app   = variable_item_get_context(item);
    uint8_t       index = variable_item_get_current_value_index(item);
    app->board_preset = (BoardPreset)index;
    if(app->board_preset != BoardPresetCustom) {
        // Load known pinout for this board
        app->gpio_config = rf_rosetta_gpio_preset(app->board_preset);
    }
    // For BoardPresetCustom, gpio_config keeps whatever was last set
    // via the custom pin editor (future enhancement) or stays at
    // the 3-in-1 default until customised.
    variable_item_set_current_value_text(item, BOARD_PRESET_NAMES[index]);
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

    // Board / GPIO preset — which dev board's pinout to use for
    // external CC1101 and NRF24 access
    item = variable_item_list_add(app->var_list, "Dev Board",
        BoardPresetCount, board_preset_change, app);
    variable_item_set_current_value_index(item, (uint8_t)app->board_preset);
    variable_item_set_current_value_text(item, BOARD_PRESET_NAMES[app->board_preset]);

    // Dwell speed
    uint8_t dwell_idx = 2; // default 300ms
    uint16_t cur_dwell = signal_capture_get_dwell(app->capture_ctx);
    for(uint8_t i = 0; i < DWELL_COUNT; i++) {
        if(DWELL_VALUES[i] <= cur_dwell) dwell_idx = i;
    }
    item = variable_item_list_add(app->var_list, "Dwell Speed",
        DWELL_COUNT, dwell_change, app);
    variable_item_set_current_value_index(item, dwell_idx);
    variable_item_set_current_value_text(item, DWELL_LABELS[dwell_idx]);

    // Logging
    item = variable_item_list_add(app->var_list, "SD Logging",
        2, logging_change, app);
    uint8_t log_idx = app->logging_enabled ? 1 : 0;
    variable_item_set_current_value_index(item, log_idx);
    variable_item_set_current_value_text(item, LOG_LABELS[log_idx]);

    // Log path — show current path (truncated) as read-only info item
    {
        const char* lp = app->log_path[0] ? app->log_path : "rf_rosetta/log.txt";
        // Show just the filename part for space
        const char* fn = lp;
        for(const char* c = lp; *c; c++) { if(*c == '/') fn = c + 1; }
        VariableItem* lpath_item = variable_item_list_add(
            app->var_list, "Log File", 1, NULL, app);
        variable_item_set_current_value_index(lpath_item, 0);
        variable_item_set_current_value_text(lpath_item, fn);
    }

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
