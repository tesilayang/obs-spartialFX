#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QApplication>
#include <QPointer>
#include <QDockWidget>
#include "dock.hpp"
OBS_DECLARE_MODULE()
MODULE_EXPORT const char *obs_module_description(void) { return "Native Spatial FX Dock and perspective compositor"; }
MODULE_EXPORT const char *obs_module_name(void) { return "OBS Spatial FX"; }
MODULE_EXPORT const char *obs_module_author(void) { return "OBS Spatial FX contributors"; }
namespace {
QPointer<SpatialDock> dock;
bool registered = false;
void frontend_event(enum obs_frontend_event event, void *) {
    if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING && !registered) {
        auto *panel = new SpatialDock();
        if (obs_frontend_add_dock_by_id("obs-spatial-fx.dock", "OBS Spatial FX", panel)) {
            dock = panel;
            registered = true;
            if (auto *nativeDock=qobject_cast<QDockWidget*>(panel->parentWidget())) { nativeDock->setFloating(false); nativeDock->show(); }
            blog(LOG_INFO, "[SpatialFX] native Qt6 dock registered after FINISHED_LOADING; phase=%d", SPATIAL_PHASE);
        } else {
            delete panel;
            blog(LOG_ERROR, "[SpatialFX] failed to register dock");
        }
    }
    if (event == OBS_FRONTEND_EVENT_EXIT || event == OBS_FRONTEND_EVENT_SCRIPTING_SHUTDOWN) {
        if (registered) {
            obs_frontend_remove_dock("obs-spatial-fx.dock");
            registered = false;
            dock.clear();
        }
    }
}
}
#if SPATIAL_PHASE >= 2
void register_spatial_stage();
#endif
bool obs_module_load(void) {
    if ((obs_get_version() >> 24) < 32) {
        blog(LOG_ERROR, "[SpatialFX] OBS32 or newer required");
        return false;
    }
#if SPATIAL_PHASE >= 2
    register_spatial_stage();
#endif
    obs_frontend_add_event_callback(frontend_event, nullptr);
    blog(LOG_INFO, "[SpatialFX] module 3.6.3 loaded, OBS=%s", obs_get_version_string());
    return true;
}
void obs_module_unload(void) {
    obs_frontend_remove_event_callback(frontend_event, nullptr);
    blog(LOG_INFO, "[SpatialFX] module unloaded");
}

