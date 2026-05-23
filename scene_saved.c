#include "rf_rosetta.h"
#include <string.h>
#include <stdio.h>

static void saved_submenu_cb(void* ctx, uint32_t index) {
    RFRosettaApp* app = ctx;
    if(index < app->saved_count) {
        app->selected_saved = (uint8_t)index;
        scene_manager_next_scene(app->scene_manager, RFRosettaSceneDetails);
    }
}

void rf_rosetta_scene_saved_on_enter(void* ctx) {
    RFRosettaApp* app = ctx;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Saved Signals");

    if(app->saved_count == 0) {
        submenu_add_item(app->submenu, "(No saved signals)", 255, NULL, NULL);
    } else {
        for(uint8_t i = 0; i < app->saved_count; i++) {
            submenu_add_item(app->submenu, app->saved[i].label, i, saved_submenu_cb, app);
        }
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, RFRosettaViewSubmenu);
}

bool rf_rosetta_scene_saved_on_event(void* ctx, SceneManagerEvent ev) {
    UNUSED(ctx);
    UNUSED(ev);
    return false;
}

void rf_rosetta_scene_saved_on_exit(void* ctx) {
    RFRosettaApp* app = ctx;
    submenu_reset(app->submenu);
}
