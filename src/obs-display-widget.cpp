#include "obs-display-widget.hpp"

#include <QEnterEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QWindow>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <algorithm>
#include <cmath>

ObsDisplayWidget::ObsDisplayWidget(QWidget *parent) : QWidget(parent) {
  setAttribute(Qt::WA_PaintOnScreen);
  setAttribute(Qt::WA_StaticContents);
  setAttribute(Qt::WA_NoSystemBackground);
  setAttribute(Qt::WA_OpaquePaintEvent);
  setAttribute(Qt::WA_DontCreateNativeAncestors);
  setAttribute(Qt::WA_NativeWindow);
  setMouseTracking(true);
  setMinimumSize(220, 124);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

ObsDisplayWidget::~ObsDisplayWidget() {
  destroyDisplay();
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
}

void ObsDisplayWidget::setPickMode(bool enabled) {
  pickMode_ = enabled;
  if (!enabled) {
    hoverX_ = -1.0f;
    hoverY_ = -1.0f;
  }
  setCursor(enabled ? Qt::CrossCursor : Qt::ArrowCursor);
}

void ObsDisplayWidget::setClickHandler(
    std::function<void(const QPointF &)> handler) {
  clickHandler_ = std::move(handler);
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
  createDisplay();
}

void ObsDisplayWidget::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  createDisplay();
  if (display_) {
    const qreal ratio = devicePixelRatioF();
    obs_display_resize(display_,
                       std::max(1, static_cast<int>(width() * ratio)),
                       std::max(1, static_cast<int>(height() * ratio)));
  }
}

void ObsDisplayWidget::mousePressEvent(QMouseEvent *event) {
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
  if (!widget || !widget->source_)
    return;

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

  if (widget->pickMode_.load())
    widget->drawMagnifier(width, height);
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
