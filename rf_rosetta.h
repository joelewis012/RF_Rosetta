#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/modules/text_box.h>
#include <gui/modules/variable_item_list.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>

#include "protocol_db.h"
#include "signal_capture.h"

// ─────────────────────────────────────────────────────────────────────────────
// Scenes
// ─────────────────────────────────────────────────────────────────────────────

typedef enum {
    RFRosettaSceneMainMenu,
    RFRosettaSceneScanning,
    RFRosettaSceneResult,
    RFRosettaSceneDetails,
    RFRosettaSceneSaved,
    RFRosettaSceneSettings,
    RFRosettaSceneCount,
} RFRosettaScene;

// ─────────────────────────────────────────────────────────────────────────────
// Views (one per scene, plus the custom scanning view)
// ─────────────────────────────────────────────────────────────────────────────

typedef enum {
    RFRosettaViewSubmenu,
    RFRosettaViewScanning,
    RFRosettaViewWidget,
    RFRosettaViewTextBox,
    RFRosettaViewVarList,
    RFRosettaViewCount,
} RFRosettaViewId;

// ─────────────────────────────────────────────────────────────────────────────
// Custom events sent from the scanning view to scene manager
// ─────────────────────────────────────────────────────────────────────────────

typedef enum {
    RFRosettaEventSignalCaught = 100,
    RFRosettaEventBackPressed,
    RFRosettaEventSaveSignal,
    RFRosettaEventViewDetails,
    RFRosettaEventScanAgain,
    RFRosettaEventDeleteSaved,
} RFRosettaEvent;

// ─────────────────────────────────────────────────────────────────────────────
// Saved signals — bookmarked captures stored to SD
// ─────────────────────────────────────────────────────────────────────────────

#define MAX_SAVED_SIGNALS 20
#define SAVED_NAME_LEN    24
#define LOG_PATH          EXT_PATH("rf_rosetta/log.txt")
#define SAVES_PATH        EXT_PATH("rf_rosetta/saved.bin")

typedef struct {
    char          label[SAVED_NAME_LEN]; // user-given name
    SignalCapture capture;
    ProtocolMatch match;
    uint32_t      timestamp;
} SavedSignal;

// ─────────────────────────────────────────────────────────────────────────────
// Application state
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    // Core GUI
    Gui*                gui;
    NotificationApp*    notifications;
    ViewDispatcher*     view_dispatcher;
    SceneManager*       scene_manager;

    // Standard views
    Submenu*            submenu;
    Widget*             widget;
    TextBox*            text_box;
    VariableItemList*   var_list;

    // Custom scanning view (drawn manually in scene_scanning.c)
    View*               scanning_view;

    // Periodic RSSI poll timer (100 ms)
    FuriTimer*          scan_timer;
    FuriMutex*          data_mutex;

    // Radio backend
    SignalCaptureCtx*   capture_ctx;

    // Current scan state
    SignalCapture       capture;
    ProtocolMatch       match;
    RSSIHistory         rssi_history;
    float               current_rssi;
    float               noise_floor;
    bool                signal_detected;
    bool                analyzing;

    // Settings
    ScanMode            scan_mode;
    AntennaMode         antenna;
    float               rssi_threshold;  // dBm — default -80

    // Saved signals
    SavedSignal         saved[MAX_SAVED_SIGNALS];
    uint8_t             saved_count;
    uint8_t             selected_saved;  // index for details view

    // Storage (for log + saves)
    Storage*            storage;
    File*               log_file;
    bool                logging_enabled;
} RFRosettaApp;

// ─────────────────────────────────────────────────────────────────────────────
// Scene handler declarations (implemented in scene_*.c)
// ─────────────────────────────────────────────────────────────────────────────

extern const SceneManagerHandlers rf_rosetta_scene_handlers;

// Main Menu
void rf_rosetta_scene_main_menu_on_enter(void* ctx);
bool rf_rosetta_scene_main_menu_on_event(void* ctx, SceneManagerEvent ev);
void rf_rosetta_scene_main_menu_on_exit(void* ctx);

// Scanning
void rf_rosetta_scene_scanning_on_enter(void* ctx);
bool rf_rosetta_scene_scanning_on_event(void* ctx, SceneManagerEvent ev);
void rf_rosetta_scene_scanning_on_exit(void* ctx);

// Result
void rf_rosetta_scene_result_on_enter(void* ctx);
bool rf_rosetta_scene_result_on_event(void* ctx, SceneManagerEvent ev);
void rf_rosetta_scene_result_on_exit(void* ctx);

// Details
void rf_rosetta_scene_details_on_enter(void* ctx);
bool rf_rosetta_scene_details_on_event(void* ctx, SceneManagerEvent ev);
void rf_rosetta_scene_details_on_exit(void* ctx);

// Saved
void rf_rosetta_scene_saved_on_enter(void* ctx);
bool rf_rosetta_scene_saved_on_event(void* ctx, SceneManagerEvent ev);
void rf_rosetta_scene_saved_on_exit(void* ctx);

// Settings
void rf_rosetta_scene_settings_on_enter(void* ctx);
bool rf_rosetta_scene_settings_on_event(void* ctx, SceneManagerEvent ev);
void rf_rosetta_scene_settings_on_exit(void* ctx);

// ─────────────────────────────────────────────────────────────────────────────
// Storage helpers (defined in rf_rosetta.c)
// ─────────────────────────────────────────────────────────────────────────────

void rf_rosetta_save_signals(RFRosettaApp* app);
void rf_rosetta_load_signals(RFRosettaApp* app);
void rf_rosetta_log_signal(RFRosettaApp* app, const SignalCapture* cap, const ProtocolMatch* match);
