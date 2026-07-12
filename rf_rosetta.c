#include "rf_rosetta.h"
#include <furi.h>
#include <furi_hal.h>
#include <storage/storage.h>
#include <string.h>
#include <stdio.h>

// ─────────────────────────────────────────────────────────────────────────────
// Navigation (back button) callback — routes back events through scene manager
// Without this, back presses from Submenu/VarList/TextBox views are swallowed
// and the user cannot navigate backwards without restarting the app.
// ─────────────────────────────────────────────────────────────────────────────

static bool rf_rosetta_navigation_event_callback(void* ctx) {
    RFRosettaApp* app = ctx;
    return scene_manager_handle_back_event(app->scene_manager);
}

// ─────────────────────────────────────────────────────────────────────────────
// Scene handler tables
// ─────────────────────────────────────────────────────────────────────────────

static void (*const scene_on_enter[])(void*) = {
    rf_rosetta_scene_main_menu_on_enter,
    rf_rosetta_scene_scanning_on_enter,
    rf_rosetta_scene_result_on_enter,
    rf_rosetta_scene_details_on_enter,
    rf_rosetta_scene_saved_on_enter,
    rf_rosetta_scene_settings_on_enter,
    rf_rosetta_scene_about_on_enter,
    rf_rosetta_scene_nrf24_on_enter,
    rf_rosetta_scene_wifi_on_enter,
};

static bool (*const scene_on_event[])(void*, SceneManagerEvent) = {
    rf_rosetta_scene_main_menu_on_event,
    rf_rosetta_scene_scanning_on_event,
    rf_rosetta_scene_result_on_event,
    rf_rosetta_scene_details_on_event,
    rf_rosetta_scene_saved_on_event,
    rf_rosetta_scene_settings_on_event,
    rf_rosetta_scene_about_on_event,
    rf_rosetta_scene_nrf24_on_event,
    rf_rosetta_scene_wifi_on_event,
};

static void (*const scene_on_exit[])(void*) = {
    rf_rosetta_scene_main_menu_on_exit,
    rf_rosetta_scene_scanning_on_exit,
    rf_rosetta_scene_result_on_exit,
    rf_rosetta_scene_details_on_exit,
    rf_rosetta_scene_saved_on_exit,
    rf_rosetta_scene_settings_on_exit,
    rf_rosetta_scene_about_on_exit,
    rf_rosetta_scene_nrf24_on_exit,
    rf_rosetta_scene_wifi_on_exit,
};

const SceneManagerHandlers rf_rosetta_scene_handlers = {
    .on_enter_handlers = scene_on_enter,
    .on_event_handlers = scene_on_event,
    .on_exit_handlers  = scene_on_exit,
    .scene_num         = RFRosettaSceneCount,
};

// ─────────────────────────────────────────────────────────────────────────────
// Storage helpers
// ─────────────────────────────────────────────────────────────────────────────

void rf_rosetta_save_signals(RFRosettaApp* app) {
    if(!app->storage) return;
    storage_common_mkdir(app->storage, EXT_PATH("rf_rosetta"));
    File* f = storage_file_alloc(app->storage);
    if(storage_file_open(f, SAVES_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(f, &app->saved_count, sizeof(app->saved_count));
        storage_file_write(f, app->saved, sizeof(SavedSignal) * app->saved_count);
        storage_file_close(f);
    }
    storage_file_free(f);
}

void rf_rosetta_load_signals(RFRosettaApp* app) {
    if(!app->storage) return;
    File* f = storage_file_alloc(app->storage);
    if(storage_file_open(f, SAVES_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_read(f, &app->saved_count, sizeof(app->saved_count));
        if(app->saved_count > MAX_SAVED_SIGNALS) app->saved_count = MAX_SAVED_SIGNALS;
        storage_file_read(f, app->saved, sizeof(SavedSignal) * app->saved_count);
        storage_file_close(f);
    }
    storage_file_free(f);
}

void rf_rosetta_log_signal(RFRosettaApp* app, const SignalCapture* cap, const ProtocolMatch* match) {
    if(!app->logging_enabled || !app->storage) return;
    storage_common_mkdir(app->storage, EXT_PATH("rf_rosetta"));
    File* f = storage_file_alloc(app->storage);
    if(storage_file_open(f, app->log_path[0] ? app->log_path : LOG_PATH,
                         FSAM_WRITE, FSOM_OPEN_APPEND)) {
        char line[128];
        snprintf(line, sizeof(line),
            "[%lu] %.0fdBm %luHz %s conf=%u%%\n",
            (unsigned long)cap->timestamp,
            (double)cap->rssi,
            (unsigned long)cap->frequency,
            match->matched ? match->protocol->short_name : "Unknown",
            match->confidence);
        storage_file_write(f, line, strlen(line));
        storage_file_close(f);
    }
    storage_file_free(f);
}

void rf_rosetta_export_sub(RFRosettaApp* app, const SignalCapture* cap, const ProtocolMatch* match) {
    if(!app->storage || !cap || cap->pulse_count == 0) return;
    storage_common_mkdir(app->storage, EXT_PATH("rf_rosetta"));

    // Build filename: /ext/rf_rosetta/<ShortName>_<timestamp>.sub
    char path[80];
    const char* proto = (match && match->matched) ? match->protocol->short_name : "RAW";
    snprintf(path, sizeof(path), EXT_PATH("rf_rosetta/%s_%lu.sub"),
             proto, (unsigned long)cap->timestamp);
    // Replace spaces in filename
    for(char* c = path + strlen(EXT_PATH("rf_rosetta/")); *c; c++) {
        if(*c == ' ') *c = '_';
    }

    File* f = storage_file_alloc(app->storage);
    if(!storage_file_open(f, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_free(f);
        return;
    }

    // Flipper Sub-GHz RAW file format
    char line[128];
    snprintf(line, sizeof(line),
        "Filetype: Flipper SubGhz RAW File\nVersion: 1\n"
        "Frequency: %lu\nPreset: FuriHalSubGhzPresetOok650Async\nProtocol: RAW\n",
        (unsigned long)cap->frequency);
    storage_file_write(f, line, strlen(line));

    // RAW_Data: alternating +high -low pulse durations in µs
    storage_file_write(f, "RAW_Data: ", 10);
    for(uint16_t i = 0; i < cap->pulse_count; i++) {
        int sign = (i % 2 == 0) ? 1 : -1;
        snprintf(line, sizeof(line), "%d ", sign * (int)cap->raw_pulses[i]);
        storage_file_write(f, line, strlen(line));
    }
    storage_file_write(f, "\n", 1);
    storage_file_close(f);
    storage_file_free(f);

    // Store path so UI can confirm
    snprintf(app->last_export_path, sizeof(app->last_export_path), "%s", path);
}

// ─────────────────────────────────────────────────────────────────────────────
// App allocation
// ─────────────────────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────────────────────
// Board GPIO presets
// ─────────────────────────────────────────────────────────────────────────────

RFGPIOConfig rf_rosetta_gpio_preset(BoardPreset preset) {
    RFGPIOConfig cfg;
    switch(preset) {
        case BoardPreset3in1:
            // Generic 3-in-1 board: CC1101 + NRF24 + ESP32 (most common)
            cfg.mosi = &gpio_ext_pa7;
            cfg.miso = &gpio_ext_pa6;
            cfg.csn  = &gpio_ext_pa4;
            cfg.sck  = &gpio_ext_pb3;
            cfg.aux  = &gpio_ext_pb2;
            break;
        case BoardPresetDevBoard:
            // Flipper official dev board CC1101 module
            cfg.mosi = &gpio_ext_pa7;
            cfg.miso = &gpio_ext_pa6;
            cfg.csn  = &gpio_ext_pa4;
            cfg.sck  = &gpio_ext_pb3;
            cfg.aux  = &gpio_ext_pc3;
            break;
        case BoardPresetWiFiDevBoard:
            // Flipper WiFi dev board v1/v2
            cfg.mosi = &gpio_ext_pa7;
            cfg.miso = &gpio_ext_pa6;
            cfg.csn  = &gpio_ext_pa4;
            cfg.sck  = &gpio_ext_pb3;
            cfg.aux  = &gpio_ext_pb2;
            break;
        case BoardPresetCC1101Breadboard:
            // Bare CC1101 module, common breadboard wiring — CSN on PC0
            cfg.mosi = &gpio_ext_pa7;
            cfg.miso = &gpio_ext_pa6;
            cfg.csn  = &gpio_ext_pc0;
            cfg.sck  = &gpio_ext_pb3;
            cfg.aux  = &gpio_ext_pb2;
            break;
        case BoardPresetNRF24Standalone:
            // Bare NRF24L01+ module — CSN on PC1
            cfg.mosi = &gpio_ext_pa7;
            cfg.miso = &gpio_ext_pa6;
            cfg.csn  = &gpio_ext_pc1;
            cfg.sck  = &gpio_ext_pb3;
            cfg.aux  = &gpio_ext_pb2;
            break;
        case BoardPresetMissileRF:
            // Rabbit-Labs Missile RF board — GDO0 on PB4
            cfg.mosi = &gpio_ext_pa7;
            cfg.miso = &gpio_ext_pa6;
            cfg.csn  = &gpio_ext_pa4;
            cfg.sck  = &gpio_ext_pb3;
            cfg.aux  = &gpio_ext_pb4;
            break;
        default:
        case BoardPresetCustom:
            // Fallback — caller should override individual pins after this
            cfg.mosi = &gpio_ext_pa7;
            cfg.miso = &gpio_ext_pa6;
            cfg.csn  = &gpio_ext_pa4;
            cfg.sck  = &gpio_ext_pb3;
            cfg.aux  = &gpio_ext_pb2;
            break;
    }
    return cfg;
}

static RFRosettaApp* rf_rosetta_alloc(void) {
    RFRosettaApp* app = malloc(sizeof(RFRosettaApp));
    furi_assert(app);
    memset(app, 0, sizeof(RFRosettaApp));

    // Defaults
    app->dwell_ticks         = 3;
    app->nrf24_timer         = NULL;
    app->antenna             = AntennaInternal;
    app->rssi_threshold      = -80.0f;
    app->logging_enabled     = true;
    app->log_path[0]         = '\0';  // empty = use default LOG_PATH
    app->last_export_path[0] = '\0';
    app->board_preset = BoardPreset3in1;
    app->gpio_config  = rf_rosetta_gpio_preset(BoardPreset3in1);

    // Core GUI
    app->gui           = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    // Scene + view dispatcher
    app->scene_manager  = scene_manager_alloc(&rf_rosetta_scene_handlers, app);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher,
        rf_rosetta_navigation_event_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Standard views
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher, RFRosettaViewSubmenu,
        submenu_get_view(app->submenu));

    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, RFRosettaViewWidget,
        widget_get_view(app->widget));

    app->text_box = text_box_alloc();
    text_box_set_font(app->text_box, TextBoxFontText);
    view_dispatcher_add_view(app->view_dispatcher, RFRosettaViewTextBox,
        text_box_get_view(app->text_box));

    app->var_list = variable_item_list_alloc();
    view_dispatcher_add_view(app->view_dispatcher, RFRosettaViewVarList,
        variable_item_list_get_view(app->var_list));

    // Custom scanning view — allocated in scene_scanning.c
    app->scanning_view = view_alloc();
    // Allocate model ONCE here — calling view_allocate_model again in on_enter
    // causes a furi_check failure on every re-entry (e.g. after Settings → Scan).
    view_allocate_model(app->scanning_view, ViewModelTypeLockFree, sizeof(ScanViewModel));
    view_dispatcher_add_view(app->view_dispatcher, RFRosettaViewScanning, app->scanning_view);

    // NRF24 view — must come AFTER view_dispatcher is allocated
    app->nrf24_view = view_alloc();
    view_allocate_model(app->nrf24_view, ViewModelTypeLockFree, sizeof(NRF24ViewModel));
    view_dispatcher_add_view(app->view_dispatcher, RFRosettaViewNRF24, app->nrf24_view);

    // WiFi/Marauder view
    app->wifi_view = view_alloc();
    view_allocate_model(app->wifi_view, ViewModelTypeLockFree, sizeof(WiFiViewModel));
    view_dispatcher_add_view(app->view_dispatcher, RFRosettaViewWifi, app->wifi_view);
    app->marauder   = NULL;  // allocated lazily on first entry to WiFi scene
    app->wifi_timer = NULL;

    // Mutex for sharing signal data between timer and UI
    app->data_mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    // Radio backend
    app->capture_ctx = signal_capture_alloc();

    // Storage
    app->storage = furi_record_open(RECORD_STORAGE);
    rf_rosetta_load_signals(app);

    return app;
}

static void rf_rosetta_free(RFRosettaApp* app) {
    furi_assert(app);

    // Stop timer if running
    if(app->scan_timer) {
        furi_timer_stop(app->scan_timer);
        furi_timer_free(app->scan_timer);
    }

    // Radio
    signal_capture_free(app->capture_ctx);

    // Remove views before freeing
    view_dispatcher_remove_view(app->view_dispatcher, RFRosettaViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, RFRosettaViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, RFRosettaViewTextBox);
    view_dispatcher_remove_view(app->view_dispatcher, RFRosettaViewVarList);
    view_dispatcher_remove_view(app->view_dispatcher, RFRosettaViewNRF24);
    view_free(app->nrf24_view);

    view_dispatcher_remove_view(app->view_dispatcher, RFRosettaViewWifi);
    view_free(app->wifi_view);
    if(app->marauder) esp32_marauder_free(app->marauder);

    view_dispatcher_remove_view(app->view_dispatcher, RFRosettaViewScanning);

    submenu_free(app->submenu);
    widget_free(app->widget);
    text_box_free(app->text_box);
    variable_item_list_free(app->var_list);
    view_free(app->scanning_view);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    furi_mutex_free(app->data_mutex);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_STORAGE);

    free(app);
}

// ─────────────────────────────────────────────────────────────────────────────
// Entry point
// ─────────────────────────────────────────────────────────────────────────────

int32_t rf_rosetta_app(void* p) {
    UNUSED(p);
    RFRosettaApp* app = rf_rosetta_alloc();

    scene_manager_next_scene(app->scene_manager, RFRosettaSceneMainMenu);
    view_dispatcher_run(app->view_dispatcher);

    rf_rosetta_free(app);
    return 0;
}
