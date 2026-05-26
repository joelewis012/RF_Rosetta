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
#include "nrf24_scanner.h"

// ─────────────────────────────────────────────────────────────────────────────
// CC1101 Scanning view model
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    float    rssi;
    float    history[RSSI_HISTORY_LEN];
    uint8_t  history_count;
    uint32_t frequency;
    bool     signal_detected;
    bool     analyzing;
    ScanMode mode;
    bool     antenna_external;
    bool     ext_not_found;
    char     freq_str[20];
    char     status_str[32];
    char     dwell_str[12];   // e.g. "Dwell: 0.3s"
    uint8_t  anim_tick;
    int8_t   rssi_trend;
    bool     seen_before;
    uint8_t  seen_count;
} ScanViewModel;

// ─────────────────────────────────────────────────────────────────────────────
// NRF24 scanning view model
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    uint8_t  hits[NRF24_CHANNELS];
    uint8_t  max_hits;
    uint32_t sweep_count;
} NRF24ViewModel;

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
    RFRosettaSceneAbout,
    RFRosettaSceneNRF24,
    RFRosettaSceneCount,
} RFRosettaScene;

// ─────────────────────────────────────────────────────────────────────────────
// Views
// ─────────────────────────────────────────────────────────────────────────────

typedef enum {
    RFRosettaViewSubmenu,
    RFRosettaViewScanning,
    RFRosettaViewWidget,
    RFRosettaViewTextBox,
    RFRosettaViewVarList,
    RFRosettaViewNRF24,
    RFRosettaViewCount,
} RFRosettaViewId;

// ─────────────────────────────────────────────────────────────────────────────
// Custom events
// ─────────────────────────────────────────────────────────────────────────────

typedef enum {
    RFRosettaEventSignalCaught = 100,
    RFRosettaEventBackPressed,
    RFRosettaEventSaveSignal,
    RFRosettaEventViewDetails,
    RFRosettaEventScanAgain,
    RFRosettaEventDeleteSaved,
    RFRosettaEventNRF24Reset,
    RFRosettaEventDwellUp,
    RFRosettaEventDwellDown,
} RFRosettaEvent;

// ─────────────────────────────────────────────────────────────────────────────
// Saved signals
// ─────────────────────────────────────────────────────────────────────────────

#define MAX_SAVED_SIGNALS 20
#define SAVED_NAME_LEN    24
#define LOG_PATH          EXT_PATH("rf_rosetta/log.txt")
#define SAVES_PATH        EXT_PATH("rf_rosetta/saved.bin")

typedef struct {
    char          label[SAVED_NAME_LEN];
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

    // CC1101 custom scanning view
    View*               scanning_view;

    // NRF24 custom scanning view
    View*               nrf24_view;

    // Timers
    FuriTimer*          scan_timer;    // CC1101 RSSI poll (100ms)
    FuriTimer*          nrf24_timer;   // NRF24 sweep (50ms) — NULL when inactive
    FuriMutex*          data_mutex;

    // CC1101 radio backend
    SignalCaptureCtx*   capture_ctx;

    // CC1101 scan state
    SignalCapture       capture;
    ProtocolMatch       match;
    RSSIHistory         rssi_history;
    float               current_rssi;
    float               noise_floor;
    bool                signal_detected;
    bool                analyzing;

    // NRF24 scan state
    NRF24ScanResult     nrf24_result;

    // Settings
    ScanMode            scan_mode;
    AntennaMode         antenna;
    float               rssi_threshold;
    uint8_t             dwell_ticks;   // 100ms ticks per freq (1-20, default 3)

    // Saved signals
    SavedSignal         saved[MAX_SAVED_SIGNALS];
    uint8_t             saved_count;
    uint8_t             selected_saved;

    // Signal fingerprinting
    uint32_t            fingerprints[64];
    uint8_t             fingerprint_counts[64];
    uint8_t             fingerprint_num;

    // Storage
    Storage*            storage;
    File*               log_file;
    bool                logging_enabled;
} RFRosettaApp;

// ─────────────────────────────────────────────────────────────────────────────
// Scene handler declarations
// ─────────────────────────────────────────────────────────────────────────────

extern const SceneManagerHandlers rf_rosetta_scene_handlers;

void rf_rosetta_scene_main_menu_on_enter(void* ctx);
bool rf_rosetta_scene_main_menu_on_event(void* ctx, SceneManagerEvent ev);
void rf_rosetta_scene_main_menu_on_exit(void* ctx);

void rf_rosetta_scene_scanning_on_enter(void* ctx);
bool rf_rosetta_scene_scanning_on_event(void* ctx, SceneManagerEvent ev);
void rf_rosetta_scene_scanning_on_exit(void* ctx);

void rf_rosetta_scene_result_on_enter(void* ctx);
bool rf_rosetta_scene_result_on_event(void* ctx, SceneManagerEvent ev);
void rf_rosetta_scene_result_on_exit(void* ctx);

void rf_rosetta_scene_details_on_enter(void* ctx);
bool rf_rosetta_scene_details_on_event(void* ctx, SceneManagerEvent ev);
void rf_rosetta_scene_details_on_exit(void* ctx);

void rf_rosetta_scene_saved_on_enter(void* ctx);
bool rf_rosetta_scene_saved_on_event(void* ctx, SceneManagerEvent ev);
void rf_rosetta_scene_saved_on_exit(void* ctx);

void rf_rosetta_scene_settings_on_enter(void* ctx);
bool rf_rosetta_scene_settings_on_event(void* ctx, SceneManagerEvent ev);
void rf_rosetta_scene_settings_on_exit(void* ctx);

void rf_rosetta_scene_about_on_enter(void* ctx);
bool rf_rosetta_scene_about_on_event(void* ctx, SceneManagerEvent ev);
void rf_rosetta_scene_about_on_exit(void* ctx);

void rf_rosetta_scene_nrf24_on_enter(void* ctx);
bool rf_rosetta_scene_nrf24_on_event(void* ctx, SceneManagerEvent ev);
void rf_rosetta_scene_nrf24_on_exit(void* ctx);

// ─────────────────────────────────────────────────────────────────────────────
// Storage helpers
// ─────────────────────────────────────────────────────────────────────────────

void rf_rosetta_save_signals(RFRosettaApp* app);
void rf_rosetta_load_signals(RFRosettaApp* app);
void rf_rosetta_log_signal(RFRosettaApp* app, const SignalCapture* cap, const ProtocolMatch* match);


// ─────────────────────────────────────────────────────────────────────────────
// Scanning view model
// Defined here (not in scene_scanning.c) so rf_rosetta.c can call
// view_allocate_model(sizeof(ScanViewModel)) at startup.
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    float    rssi;
    float    history[RSSI_HISTORY_LEN];
    uint8_t  history_count;
    uint32_t frequency;
    bool     signal_detected;
    bool     analyzing;
    ScanMode mode;
    bool     antenna_external;
    bool     ext_not_found;      // true when external was requested but not connected
    char     freq_str[20];
    char     status_str[32];
    uint8_t  anim_tick;
    int8_t   rssi_trend;         // +1 = stronger, -1 = weaker, 0 = steady
    bool     seen_before;        // fingerprint: this device seen in this session
    uint8_t  seen_count;         // how many times seen
} ScanViewModel;

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
    RFRosettaSceneAbout,
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

    // Signal fingerprinting — hash of (freq, modulation, pulse_avg) seen this session
    uint32_t            fingerprints[64];
    uint8_t             fingerprint_counts[64];
    uint8_t             fingerprint_num;

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

// About
void rf_rosetta_scene_about_on_enter(void* ctx);
bool rf_rosetta_scene_about_on_event(void* ctx, SceneManagerEvent ev);
void rf_rosetta_scene_about_on_exit(void* ctx);

// ─────────────────────────────────────────────────────────────────────────────
// Storage helpers (defined in rf_rosetta.c)
// ─────────────────────────────────────────────────────────────────────────────

void rf_rosetta_save_signals(RFRosettaApp* app);
void rf_rosetta_load_signals(RFRosettaApp* app);
void rf_rosetta_log_signal(RFRosettaApp* app, const SignalCapture* cap, const ProtocolMatch* match);
