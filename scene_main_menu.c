#include "rf_rosetta.h"

typedef enum {
    MenuScan,
    MenuSaved,
    MenuSettings,
} MenuItem;

static void menu_callback(void* ctx, uint32_t index) {
    RFRosettaApp* app = ctx;
    switch(index) {
        case MenuScan:
            scene_manager_next_scene(app->scene_manager, RFRosettaSceneScanning);
            break;
        case MenuSaved:
            scene_manager_next_scene(app->scene_manager, RFRosettaSceneSaved);
            break;
        case MenuSettings:
            scene_manager_next_scene(app->scene_manager, RFRosettaSceneSettings);
            break;
    }
}

void rf_rosetta_scene_main_menu_on_enter(void* ctx) {
    RFRosettaApp* app = ctx;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "RF Rosetta");
    submenu_add_item(app->submenu, "Scan for Signals", MenuScan, menu_callback, app);
    submenu_add_item(app->submenu, "Saved Signals",    MenuSaved, menu_callback, app);
    submenu_add_item(app->submenu, "Settings",         MenuSettings, menu_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, RFRosettaViewSubmenu);
}

bool rf_rosetta_scene_main_menu_on_event(void* ctx, SceneManagerEvent ev) {
    UNUSED(ctx);
    UNUSED(ev);
    return false;
}

void rf_rosetta_scene_main_menu_on_exit(void* ctx) {
    RFRosettaApp* app = ctx;
    submenu_reset(app->submenu);
}
