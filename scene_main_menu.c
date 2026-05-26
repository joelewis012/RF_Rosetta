#include "rf_rosetta.h"

typedef enum {
    MenuScan,
    MenuNRF24,
    MenuSaved,
    MenuSettings,
    MenuAbout,
} MenuItem;

static void menu_callback(void* ctx, uint32_t index) {
    RFRosettaApp* app = ctx;
    switch(index) {
        case MenuScan:     scene_manager_next_scene(app->scene_manager, RFRosettaSceneScanning); break;
        case MenuNRF24:    scene_manager_next_scene(app->scene_manager, RFRosettaSceneNRF24);    break;
        case MenuSaved:    scene_manager_next_scene(app->scene_manager, RFRosettaSceneSaved);    break;
        case MenuSettings: scene_manager_next_scene(app->scene_manager, RFRosettaSceneSettings); break;
        case MenuAbout:    scene_manager_next_scene(app->scene_manager, RFRosettaSceneAbout);    break;
    }
}

void rf_rosetta_scene_main_menu_on_enter(void* ctx) {
    RFRosettaApp* app = ctx;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "RF Rosetta");
    submenu_add_item(app->submenu, "CC1101 Sub-GHz Scan", MenuScan,     menu_callback, app);
    submenu_add_item(app->submenu, "NRF24 2.4GHz Scan",   MenuNRF24,   menu_callback, app);
    submenu_add_item(app->submenu, "Saved Signals",        MenuSaved,   menu_callback, app);
    submenu_add_item(app->submenu, "Settings",             MenuSettings,menu_callback, app);
    submenu_add_item(app->submenu, "About",                MenuAbout,   menu_callback, app);
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
