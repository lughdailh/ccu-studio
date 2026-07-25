#import <AppKit/AppKit.h>

#include <obs-module.h>
#include <obs.h>
#include <util/platform.h>

#include <QApplication>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

namespace {
const char *testName(void *) { return "CCU test source"; }
uint32_t testWidth(void *) { return 64; }
uint32_t testHeight(void *) { return 64; }
struct TestSource {
  gs_texture_t *texture = nullptr;
};
void *testCreate(obs_data_t *, obs_source_t *) {
  auto *context = new TestSource;
  const uint8_t pixel[] = {51, 102, 153, 255};
  const uint8_t *planes[] = {pixel};
  obs_enter_graphics();
  context->texture = gs_texture_create(1, 1, GS_RGBA, 1, planes, 0);
  obs_leave_graphics();
  return context->texture ? context : nullptr;
}
void testDestroy(void *data) {
  auto *context = static_cast<TestSource *>(data);
  obs_enter_graphics();
  gs_texture_destroy(context->texture);
  obs_leave_graphics();
  delete context;
}
void testRender(void *data, gs_effect_t *effect) {
  auto *context = static_cast<TestSource *>(data);
  gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"),
                        context->texture);
  gs_draw_sprite(context->texture, 0, 64, 64);
}

std::array<uint8_t, 4> renderPixel(obs_source_t *source) {
  std::array<uint8_t, 4> result{};
  obs_enter_graphics();
  gs_texrender_t *render = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
  gs_stagesurf_t *stage = gs_stagesurface_create(64, 64, GS_RGBA);
  if (render && stage && gs_texrender_begin(render, 64, 64)) {
    vec4 clear{};
    gs_clear(GS_CLEAR_COLOR, &clear, 0.0f, 0);
    gs_ortho(0.0f, 64.0f, 0.0f, 64.0f, -100.0f, 100.0f);
    obs_source_video_render(source);
    gs_texrender_end(render);
    gs_stage_texture(stage, gs_texrender_get_texture(render));
    gs_flush();
    uint8_t *pixels = nullptr;
    uint32_t stride = 0;
    if (gs_stagesurface_map(stage, &pixels, &stride)) {
      const uint8_t *center = pixels + 32 * stride + 32 * 4;
      std::copy(center, center + 4, result.begin());
      gs_stagesurface_unmap(stage);
    }
  }
  gs_stagesurface_destroy(stage);
  gs_texrender_destroy(render);
  obs_leave_graphics();
  return result;
}
} // namespace

int main(int argc, char **argv) {
  @autoreleasepool {
    QApplication application(argc, argv);
    if (argc != 4 || !obs_startup("ca-ES", nullptr, nullptr))
      return 2;

    obs_video_info video{};
    video.graphics_module = argv[1];
    video.fps_num = 30;
    video.fps_den = 1;
    video.base_width = 640;
    video.base_height = 360;
    video.output_width = 640;
    video.output_height = 360;
    video.output_format = VIDEO_FORMAT_RGBA;
    video.colorspace = VIDEO_CS_709;
    video.range = VIDEO_RANGE_FULL;
    video.scale_type = OBS_SCALE_BILINEAR;
    if (obs_reset_video(&video) != OBS_VIDEO_SUCCESS)
      return 3;

    obs_module_t *module = nullptr;
    if (obs_open_module(&module, argv[2], argv[3]) != MODULE_SUCCESS ||
        !module || !obs_init_module(module))
      return 4;

    obs_source_info testInfo{};
    testInfo.id = "ccu_test_source";
    testInfo.type = OBS_SOURCE_TYPE_INPUT;
    testInfo.output_flags = OBS_SOURCE_VIDEO;
    testInfo.get_name = testName;
    testInfo.create = testCreate;
    testInfo.destroy = testDestroy;
    testInfo.get_width = testWidth;
    testInfo.get_height = testHeight;
    testInfo.video_render = testRender;
    obs_register_source(&testInfo);

    obs_source_t *source =
        obs_source_create_private("ccu_test_source", "test-source", nullptr);
    obs_source_t *filter =
        obs_source_create_private("ccu_obs_color_filter", "smoke", nullptr);
    if (!source || !filter)
      return 5;

    const auto before = renderPixel(source);
    obs_data_t *settings = obs_source_get_settings(filter);
    obs_data_set_double(settings, "red_gain", 1.8);
    obs_data_set_double(settings, "green_gain", 0.7);
    obs_data_set_double(settings, "blue_gain", 1.2);
    obs_source_update(filter, settings);
    obs_data_release(settings);
    obs_source_filter_add(source, filter);
    os_sleep_ms(80);
    const auto after = renderPixel(source);
    if (std::abs(int(after[0]) - int(before[0])) < 15 ||
        std::abs(int(after[1]) - int(before[1])) < 15) {
      std::fprintf(stderr,
                   "filter pixels unchanged: before=%u,%u,%u after=%u,%u,%u\n",
                   before[0], before[1], before[2], after[0], after[1],
                   after[2]);
      return 6;
    }

    obs_source_release(filter);
    obs_source_release(source);
    obs_shutdown();
    return 0;
  }
}
