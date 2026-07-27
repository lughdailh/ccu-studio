#include "obs-display-widget.hpp"

#include <obs-module.h>

#include <QEnterEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QWindow>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
// Without NOMINMAX, windows.h's min/max macros mangle every std::max/
// std::clamp call in this file at the preprocessor level (MSVC C2589/C2059).
#define NOMINMAX
#include <windows.h>
#endif

#include <algorithm>
#include <cmath>

ObsDisplayWidget::ObsDisplayWidget(QWidget *parent) : QWidget(parent) {
  const float defaults[] = {1, 1, 1, 0, 1, 1, 1};
  for (size_t i = 0; i < compareSettings_.size(); ++i)
    compareSettings_[i] = defaults[i];
  // Keep the same native-widget hierarchy used by OBS's own Qt display
  // surfaces. WA_DontCreateNativeAncestors made these four NSViews siblings
  // of their logical Qt containers on macOS; after a layout resize their
  // frames could move while the OBS surfaces stayed at the old window
  // coordinates and painted over unrelated controls.
  setAttribute(Qt::WA_NativeWindow);
  setAttribute(Qt::WA_PaintOnScreen);
  setAttribute(Qt::WA_NoSystemBackground);
  setAttribute(Qt::WA_OpaquePaintEvent);
  setAutoFillBackground(false);
  setMouseTracking(true);
  // The CCU can be reduced on smaller displays. A native child minimum here
  // propagates through Qt and silently becomes the minimum size of the whole
  // dialog, so the mosaic itself decides the practical preview size.
  setMinimumSize(1, 1);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  auto *hint = new QLabel(this);
  emptyHint_ = hint;
  hint->setAttribute(Qt::WA_TransparentForMouseEvents);
  hint->setAlignment(Qt::AlignCenter);
  hint->setWordWrap(true);
  hint->setStyleSheet(
      QStringLiteral("QLabel { color: #808080; background: transparent; }"));
  const QString instruction =
      QString::fromUtf8(obs_module_text("EmptyPreviewHint")).toHtmlEscaped();
  hint->setText(
      QStringLiteral("<div style=\"text-align:center; color:#808080;\">"
                     "<span style=\"font-size:48px; font-weight:300;\">⊕</span>"
                     "<br><span style=\"font-size:14px;\">%1</span></div>")
          .arg(instruction));
}

ObsDisplayWidget::~ObsDisplayWidget() {
  destroyDisplay();
  destroyCompareResources();
  if (source_) {
    obs_source_dec_showing(source_);
    obs_source_release(source_);
  }
}

void ObsDisplayWidget::setSource(obs_source_t *source) {
  if (source == source_)
    return;
  if (source_) {
    obs_source_dec_showing(source_);
    obs_source_release(source_);
  }
  source_ = source ? obs_source_get_ref(source) : nullptr;
  if (source_)
    obs_source_inc_showing(source_);
  if (emptyHint_) {
    emptyHint_->setVisible(!source_);
    emptyHint_->raise();
  }
}

void ObsDisplayWidget::setPickMode(bool enabled) {
  pickMode_ = enabled;
  if (!enabled) {
    hoverX_ = -1.0f;
    hoverY_ = -1.0f;
  }
  setCursor(enabled ? Qt::CrossCursor : Qt::ArrowCursor);
}

void ObsDisplayWidget::setSelected(bool selected) { selected_ = selected; }

void ObsDisplayWidget::setCompareMode(
    bool enabled, const std::array<float, 7> &settings) {
  for (size_t i = 0; i < settings.size(); ++i)
    compareSettings_[i] = settings[i];
  compareMode_ = enabled;
}

void ObsDisplayWidget::setClickHandler(
    std::function<void(const QPointF &)> handler) {
  clickHandler_ = std::move(handler);
}

void ObsDisplayWidget::setContextMenuHandler(
    std::function<void(const QPoint &)> handler) {
  contextMenuHandler_ = std::move(handler);
}

int ObsDisplayWidget::heightForWidth(int width) const {
  return std::max(1, static_cast<int>(std::lround(width * 9.0 / 16.0)));
}

QSize ObsDisplayWidget::sizeHint() const {
  return {320, heightForWidth(320)};
}

QRectF ObsDisplayWidget::videoRect() const {
  if (!source_)
    return {};
  const uint32_t sourceWidth = obs_source_get_width(source_);
  const uint32_t sourceHeight = obs_source_get_height(source_);
  if (!sourceWidth || !sourceHeight)
    return {};
  const QSize scaled =
      QSize(static_cast<int>(sourceWidth), static_cast<int>(sourceHeight))
          .scaled(size(), Qt::KeepAspectRatio);
  return {(width() - scaled.width()) / 2.0,
          (height() - scaled.height()) / 2.0,
          static_cast<double>(scaled.width()),
          static_cast<double>(scaled.height())};
}

void ObsDisplayWidget::createDisplay() {
  if (display_ || !isVisible() || !windowHandle() ||
      !windowHandle()->isExposed())
    return;

  const qreal ratio = devicePixelRatioF();
  gs_init_data info{};
  info.cx = std::max(1, static_cast<int>(width() * ratio));
  info.cy = std::max(1, static_cast<int>(height() * ratio));
  info.format = GS_BGRA;
  info.zsformat = GS_ZS_NONE;
#ifdef _WIN32
  info.window.hwnd = reinterpret_cast<HWND>(winId());
#elif defined(__APPLE__)
  info.window.view = reinterpret_cast<id>(winId());
#endif
  display_ = obs_display_create(&info, 0xFF111111);
  if (display_)
    obs_display_add_draw_callback(display_, draw, this);
}

void ObsDisplayWidget::destroyDisplay() {
  if (!display_)
    return;
  obs_display_remove_draw_callback(display_, draw, this);
  obs_display_destroy(display_);
  display_ = nullptr;
}

void ObsDisplayWidget::paintEvent(QPaintEvent *) { createDisplay(); }

void ObsDisplayWidget::showEvent(QShowEvent *event) {
  QWidget::showEvent(event);
  if (emptyHint_)
    emptyHint_->raise();
  createDisplay();
}

void ObsDisplayWidget::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  if (emptyHint_) {
    const int hintWidth = std::min(width(), 360);
    emptyHint_->setGeometry((width() - hintWidth) / 2, 0, hintWidth, height());
    emptyHint_->raise();
  }
  createDisplay();
  if (display_) {
    const qreal ratio = devicePixelRatioF();
    obs_display_resize(display_,
                       std::max(1, static_cast<int>(width() * ratio)),
                       std::max(1, static_cast<int>(height() * ratio)));
  }
}

void ObsDisplayWidget::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::RightButton && contextMenuHandler_) {
    contextMenuHandler_(mapToGlobal(event->position().toPoint()));
    return;
  }
  if (event->button() == Qt::LeftButton && clickHandler_) {
    const QRectF image = videoRect();
    if (image.contains(event->position())) {
      const QPointF normalized(
          std::clamp((event->position().x() - image.left()) / image.width(),
                     0.0, 1.0),
          std::clamp((event->position().y() - image.top()) / image.height(),
                     0.0, 1.0));
      clickHandler_(normalized);
      return;
    }
  }
  QWidget::mousePressEvent(event);
}

void ObsDisplayWidget::mouseMoveEvent(QMouseEvent *event) {
  if (pickMode_) {
    const QRectF image = videoRect();
    const bool inside = image.contains(event->position());
    setCursor(inside ? Qt::CrossCursor : Qt::ArrowCursor);
    if (inside) {
      hoverX_ = static_cast<float>(
          (event->position().x() - image.left()) / image.width());
      hoverY_ = static_cast<float>(
          (event->position().y() - image.top()) / image.height());
    } else {
      hoverX_ = -1.0f;
      hoverY_ = -1.0f;
    }
  }
  QWidget::mouseMoveEvent(event);
}

void ObsDisplayWidget::enterEvent(QEnterEvent *event) {
  if (pickMode_ && !videoRect().contains(event->position()))
    setCursor(Qt::ArrowCursor);
  QWidget::enterEvent(event);
}

void ObsDisplayWidget::leaveEvent(QEvent *event) {
  if (pickMode_) {
    setCursor(Qt::ArrowCursor);
    hoverX_ = -1.0f;
    hoverY_ = -1.0f;
  }
  QWidget::leaveEvent(event);
}

void ObsDisplayWidget::draw(void *data, uint32_t width, uint32_t height) {
  auto *widget = static_cast<ObsDisplayWidget *>(data);
  if (!widget)
    return;

  if (widget->source_) {
    const uint32_t sourceWidth =
        std::max(obs_source_get_width(widget->source_), 1u);
    const uint32_t sourceHeight =
        std::max(obs_source_get_height(widget->source_), 1u);
    const float scale =
        std::min(static_cast<float>(width) / sourceWidth,
                 static_cast<float>(height) / sourceHeight);
    const int outputWidth = static_cast<int>(sourceWidth * scale);
    const int outputHeight = static_cast<int>(sourceHeight * scale);
    const int x = (static_cast<int>(width) - outputWidth) / 2;
    const int y = (static_cast<int>(height) - outputHeight) / 2;

    gs_viewport_push();
    gs_projection_push();
    const bool previous = gs_set_linear_srgb(true);
    gs_ortho(0.0f, static_cast<float>(sourceWidth), 0.0f,
             static_cast<float>(sourceHeight), -100.0f, 100.0f);
    gs_set_viewport(x, y, outputWidth, outputHeight);
    obs_source_video_render(widget->source_);
    gs_set_linear_srgb(previous);
    gs_projection_pop();
    gs_viewport_pop();

    if (widget->compareMode_.load())
      widget->drawComparison(width, height, x, y, outputWidth, outputHeight,
                             sourceWidth, sourceHeight);

    if (widget->pickMode_.load())
      widget->drawMagnifier(width, height);
  }

  if (widget->selected_.load()) {
    const vec4 gold = {0.902f, 0.706f, 0.133f, 1.0f};
    constexpr float thickness = 1.0f;
    widget->drawSolidRect(0, 0, static_cast<float>(width), thickness, width,
                          height, gold);
    widget->drawSolidRect(0, static_cast<float>(height) - thickness,
                          static_cast<float>(width), thickness, width, height,
                          gold);
    widget->drawSolidRect(0, 0, thickness, static_cast<float>(height), width,
                          height, gold);
    widget->drawSolidRect(static_cast<float>(width) - thickness, 0, thickness,
                          static_cast<float>(height), width, height, gold);
  }
}

bool ObsDisplayWidget::ensureCompareResources() {
  if (!compareRender_)
    compareRender_ = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
  if (!compareEffect_) {
    char *path = obs_module_file("ccu-compare.effect");
    char *errors = nullptr;
    compareEffect_ =
        path ? gs_effect_create_from_file(path, &errors) : nullptr;
    if (!compareEffect_)
      blog(LOG_ERROR, "[CCU OBS] Compare shader error: %s",
           errors ? errors : "effect not found");
    bfree(errors);
    bfree(path);
  }
  return compareRender_ && compareEffect_;
}

void ObsDisplayWidget::destroyCompareResources() {
  if (!compareRender_ && !compareEffect_)
    return;
  obs_enter_graphics();
  gs_texrender_destroy(compareRender_);
  gs_effect_destroy(compareEffect_);
  compareRender_ = nullptr;
  compareEffect_ = nullptr;
  obs_leave_graphics();
}

void ObsDisplayWidget::drawComparison(
    uint32_t canvasWidth, uint32_t canvasHeight, int x, int y,
    int outputWidth, int outputHeight, uint32_t sourceWidth,
    uint32_t sourceHeight) {
  if (!source_ || outputWidth < 2 || outputHeight < 1 ||
      !ensureCompareResources())
    return;

  gs_texrender_reset(compareRender_);
  if (!gs_texrender_begin_with_color_space(compareRender_, sourceWidth,
                                           sourceHeight, GS_CS_SRGB))
    return;
  vec4 clear{};
  gs_clear(GS_CLEAR_COLOR, &clear, 0.0f, 0);
  gs_ortho(0.0f, static_cast<float>(sourceWidth), 0.0f,
           static_cast<float>(sourceHeight), -100.0f, 100.0f);
  gs_set_viewport(0, 0, sourceWidth, sourceHeight);
  obs_source_video_render(source_);
  gs_texrender_end(compareRender_);

  gs_texture_t *texture = gs_texrender_get_texture(compareRender_);
  if (!texture)
    return;
  const char *names[] = {"red_gain",  "green_gain", "blue_gain",
                         "brightness", "contrast",   "gamma_value",
                         "saturation"};
  for (size_t i = 0; i < compareSettings_.size(); ++i) {
    if (gs_eparam_t *parameter =
            gs_effect_get_param_by_name(compareEffect_, names[i]))
      gs_effect_set_float(parameter, compareSettings_[i].load());
  }
  if (gs_eparam_t *image =
          gs_effect_get_param_by_name(compareEffect_, "image"))
    gs_effect_set_texture(image, texture);

  gs_viewport_push();
  gs_projection_push();
  gs_matrix_push();
  gs_ortho(0.0f, static_cast<float>(canvasWidth), 0.0f,
           static_cast<float>(canvasHeight), -100.0f, 100.0f);
  gs_set_viewport(0, 0, canvasWidth, canvasHeight);
  gs_matrix_identity();
  gs_matrix_translate3f(static_cast<float>(x), static_cast<float>(y), 0.0f);
  const gs_rect leftHalf{x, y, outputWidth / 2, outputHeight};
  gs_set_scissor_rect(&leftHalf);
  while (gs_effect_loop(compareEffect_, "Draw"))
    gs_draw_sprite(texture, 0, static_cast<uint32_t>(outputWidth),
                   static_cast<uint32_t>(outputHeight));
  gs_set_scissor_rect(nullptr);
  gs_matrix_pop();
  gs_projection_pop();
  gs_viewport_pop();

  const vec4 divider = {0.92f, 0.92f, 0.92f, 0.86f};
  drawSolidRect(x + outputWidth / 2.0f, static_cast<float>(y), 1.0f,
                static_cast<float>(outputHeight), canvasWidth, canvasHeight,
                divider);
}

void ObsDisplayWidget::drawSolidRect(float x, float y, float width,
                                     float height, uint32_t canvasWidth,
                                     uint32_t canvasHeight,
                                     const vec4 &colorValue) {
  gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
  if (!solid)
    return;
  gs_eparam_t *color = gs_effect_get_param_by_name(solid, "color");
  gs_effect_set_vec4(color, &colorValue);
  gs_viewport_push();
  gs_projection_push();
  gs_matrix_push();
  gs_ortho(0.0f, static_cast<float>(canvasWidth), 0.0f,
           static_cast<float>(canvasHeight), -100.0f, 100.0f);
  gs_set_viewport(0, 0, canvasWidth, canvasHeight);
  gs_matrix_identity();
  gs_matrix_translate3f(x, y, 0.0f);
  while (gs_effect_loop(solid, "Solid"))
    gs_draw_sprite(nullptr, 0, static_cast<uint32_t>(width),
                   static_cast<uint32_t>(height));
  gs_matrix_pop();
  gs_projection_pop();
  gs_viewport_pop();
}

void ObsDisplayWidget::drawMagnifier(uint32_t width, uint32_t height) {
  const float normalizedX = hoverX_.load();
  const float normalizedY = hoverY_.load();
  if (!source_ || normalizedX < 0.0f || normalizedY < 0.0f)
    return;

  const uint32_t sourceWidth = std::max(obs_source_get_width(source_), 1u);
  const uint32_t sourceHeight = std::max(obs_source_get_height(source_), 1u);
  const float previewScale =
      std::min(static_cast<float>(width) / sourceWidth,
               static_cast<float>(height) / sourceHeight);
  const float previewWidth = sourceWidth * previewScale;
  const float previewHeight = sourceHeight * previewScale;
  const float pointerX = (width - previewWidth) / 2.0f +
                         normalizedX * previewWidth;
  const float pointerY = (height - previewHeight) / 2.0f +
                         normalizedY * previewHeight;

  const float size = std::min(190.0f, std::min(width, height) * 0.72f);
  const float margin = 14.0f;
  float left = pointerX + margin;
  float top = pointerY - size - margin;
  if (left + size > width)
    left = pointerX - size - margin;
  if (top < 0.0f)
    top = pointerY + margin;
  left = std::clamp(left, 0.0f, std::max(0.0f, width - size));
  top = std::clamp(top, 0.0f, std::max(0.0f, height - size));

  const float cropSize =
      std::max(12.0f, static_cast<float>(std::min(sourceWidth, sourceHeight)) /
                            18.0f);
  const float centerX = normalizedX * sourceWidth;
  const float centerY = normalizedY * sourceHeight;
  float cropLeft =
      std::clamp(centerX - cropSize / 2.0f, 0.0f,
                 std::max(0.0f, sourceWidth - cropSize));
  float cropTop =
      std::clamp(centerY - cropSize / 2.0f, 0.0f,
                 std::max(0.0f, sourceHeight - cropSize));

  gs_viewport_push();
  gs_projection_push();
  gs_ortho(cropLeft, cropLeft + cropSize, cropTop, cropTop + cropSize,
           -100.0f, 100.0f);
  gs_set_viewport(static_cast<int>(left), static_cast<int>(top),
                  static_cast<int>(size), static_cast<int>(size));
  obs_source_video_render(source_);
  gs_projection_pop();
  gs_viewport_pop();

  gs_blend_state_push();
  gs_blend_function(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA);
  vec4 translucent{};
  vec4_set(&translucent, 1.0f, 1.0f, 1.0f, 0.48f);
  vec4 white{};
  vec4_set(&white, 1.0f, 1.0f, 1.0f, 1.0f);
  vec4 black{};
  vec4_set(&black, 0.0f, 0.0f, 0.0f, 0.85f);
  drawSolidRect(left, top, size, 2.0f, width, height, white);
  drawSolidRect(left, top + size - 2.0f, size, 2.0f, width, height, white);
  drawSolidRect(left, top, 2.0f, size, width, height, white);
  drawSolidRect(left + size - 2.0f, top, 2.0f, size, width, height, white);
  drawSolidRect(left, top + size / 2.0f - 1.0f, size, 2.0f, width, height,
                translucent);
  drawSolidRect(left + size / 2.0f - 1.0f, top, 2.0f, size, width, height,
                translucent);
  drawSolidRect(left + size / 2.0f - 5.0f,
                top + size / 2.0f - 5.0f, 10.0f, 10.0f, width, height,
                black);
  drawSolidRect(left + size / 2.0f - 3.0f,
                top + size / 2.0f - 3.0f, 6.0f, 6.0f, width, height,
                white);
  gs_blend_state_pop();
}
