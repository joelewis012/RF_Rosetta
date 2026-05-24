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
};

static bool (*const scene_on_event[])(void*, SceneManagerEvent) = {
    rf_rosetta_scene_main_menu_on_event,
    rf_rosetta_scene_scanning_on_event,
    rf_rosetta_scene_result_on_event,
    rf_rosetta_scene_details_on_event,
    rf_rosetta_scene_saved_on_event,
    rf_rosetta_scene_settings_on_event,
};

static void (*const scene_on_exit[])(void*) = {
    rf_rosetta_scene_main_menu_on_exit,
    rf_rosetta_scene_scanning_on_exit,
    rf_rosetta_scene_result_on_exit,
    rf_rosetta_scene_details_on_exit,
    rf_rosetta_scene_saved_on_exit,
    rf_rosetta_scene_settings_on_exit,
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
    if(storage_file_open(f, LOG_PATH, FSAM_WRITE, FSOM_OPEN_APPEND)) {
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

// ─────────────────────────────────────────────────────────────────────────────
// App allocation
// ─────────────────────────────────────────────────────────────────────────────

static RFRosettaApp* rf_rosetta_alloc(void) {
    RFRosettaApp* app = malloc(sizeof(RFRosettaApp));
    furi_assert(app);
    memset(app, 0, sizeof(RFRosettaApp));

    // Defaults
    app->scan_mode      = ScanModeSubGHz;
    app->antenna        = AntennaInternal;
    app->rssi_threshold = -80.0f;
    app->logging_enabled = true;

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
    view_dispatcher_add_view(app->view_dispatcher, RFRosettaViewScanning, app->scanning_view);

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
