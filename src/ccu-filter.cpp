#include "ccu-window.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QAction>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-ccu", "ca-ES")

namespace {
struct Filter {
  obs_source_t *source = nullptr;
  gs_effect_t *effect = nullptr;
  gs_eparam_t *red = nullptr;
  gs_eparam_t *green = nullptr;
  gs_eparam_t *blue = nullptr;
  gs_eparam_t *brightness = nullptr;
  gs_eparam_t *contrast = nullptr;
  gs_eparam_t *gamma = nullptr;
  gs_eparam_t *saturation = nullptr;
  float values[7]{1, 1, 1, 0, 1, 1, 1};
};

constexpr const char *keys[] = {"red_gain",  "green_gain", "blue_gain",
                                "brightness", "contrast",   "gamma_value",
                                "saturation"};

const char *filterName(void *) { return "CCU OBS — correcció de color"; }

void filterDefaults(obs_data_t *settings) {
  obs_data_set_default_double(settings, keys[0], 1.0);
  obs_data_set_default_double(settings, keys[1], 1.0);
  obs_data_set_default_double(settings, keys[2], 1.0);
  obs_data_set_default_double(settings, keys[3], 0.0);
  obs_data_set_default_double(settings, keys[4], 1.0);
  obs_data_set_default_double(settings, keys[5], 1.0);
  obs_data_set_default_double(settings, keys[6], 1.0);
}
void filterUpdate(void *data, obs_data_t *settings) {
  auto *filter = static_cast<Filter *>(data);
  if (!filter)
    return;
  for (size_t i = 0; i < 7; ++i)
    filter->values[i] = static_cast<float>(obs_data_get_double(settings, keys[i]));
}

void *filterCreate(obs_data_t *settings, obs_source_t *source) {
  auto *filter = new Filter;
  filter->source = source;
  char *path = obs_module_file("ccu-color.effect");
  char *errors = nullptr;
  obs_enter_graphics();
  filter->effect = path ? gs_effect_create_from_file(path, &errors) : nullptr;
  obs_leave_graphics();
  bfree(path);
  if (!filter->effect) {
    blog(LOG_ERROR, "[CCU OBS] Shader error: %s",
         errors ? errors : "effect not found");
    bfree(errors);
    delete filter;
    return nullptr;
  }
  bfree(errors);
  gs_eparam_t **parameters[] = {
      &filter->red,        &filter->green, &filter->blue,
      &filter->brightness, &filter->contrast, &filter->gamma,
      &filter->saturation};
  for (size_t i = 0; i < 7; ++i)
    *parameters[i] = gs_effect_get_param_by_name(filter->effect, keys[i]);
  filterUpdate(filter, settings);
  return filter;
}

void filterDestroy(void *data) {
  auto *filter = static_cast<Filter *>(data);
  if (!filter)
    return;
  obs_enter_graphics();
  gs_effect_destroy(filter->effect);
  obs_leave_graphics();
  delete filter;
}

void filterRender(void *data, gs_effect_t *) {
  auto *filter = static_cast<Filter *>(data);
  if (!filter || !filter->effect ||
      !obs_source_process_filter_begin(filter->source, GS_RGBA,
                                       OBS_ALLOW_DIRECT_RENDERING))
    return;
  gs_eparam_t *parameters[] = {
      filter->red,        filter->green, filter->blue, filter->brightness,
      filter->contrast,   filter->gamma, filter->saturation};
  for (size_t i = 0; i < 7; ++i)
    gs_effect_set_float(parameters[i], filter->values[i]);
  obs_source_process_filter_end(filter->source, filter->effect, 0, 0);
}

obs_source_info makeFilterInfo() {
  obs_source_info info{};
  info.id = "ccu_obs_color_filter";
  info.type = OBS_SOURCE_TYPE_FILTER;
  info.output_flags = OBS_SOURCE_VIDEO;
  info.get_name = filterName;
  info.get_defaults = filterDefaults;
  info.create = filterCreate;
  info.destroy = filterDestroy;
  info.update = filterUpdate;
  info.video_render = filterRender;
  return info;
}

obs_source_info filterInfo = makeFilterInfo();
QAction *toolsAction = nullptr;

void frontendEvent(enum obs_frontend_event event, void *) {
  if (event == OBS_FRONTEND_EVENT_EXIT)
    closeCcuWindow();
  else if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED)
    closeCcuWindow();
}
} // namespace

MODULE_EXPORT const char *obs_module_description(void) {
  return "Four-camera CCU window for OBS Studio";
}

bool obs_module_load(void) {
  obs_register_source(&filterInfo);
  toolsAction = static_cast<QAction *>(
      obs_frontend_add_tools_menu_qaction("CCU OBS…"));
  QObject::connect(toolsAction, &QAction::triggered, [] { showCcuWindow(); });
  obs_frontend_add_event_callback(frontendEvent, nullptr);
  blog(LOG_INFO, "[CCU OBS] Loaded version %s", CCU_VERSION);
  return true;
}

void obs_module_unload(void) {
  obs_frontend_remove_event_callback(frontendEvent, nullptr);
  closeCcuWindow();
}
