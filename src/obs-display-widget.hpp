#pragma once

#include <obs.h>

#include <QPointF>
#include <QWidget>

#include <functional>
#include <atomic>
#include <array>

class ObsDisplayWidget final : public QWidget {
public:
  explicit ObsDisplayWidget(QWidget *parent = nullptr);
  ~ObsDisplayWidget() override;

  void setSource(obs_source_t *source);
  obs_source_t *source() const { return source_; }
  QRectF videoRect() const;
  void setPickMode(bool enabled);
  void setSelected(bool selected);
  void setCompareMode(bool enabled, const std::array<float, 7> &settings);
  void setClickHandler(std::function<void(const QPointF &)> handler);
  void setContextMenuHandler(std::function<void(const QPoint &)> handler);
  bool hasHeightForWidth() const override { return true; }
  int heightForWidth(int width) const override;
  QSize sizeHint() const override;

protected:
  QPaintEngine *paintEngine() const override { return nullptr; }
  void paintEvent(QPaintEvent *) override;
  void resizeEvent(QResizeEvent *) override;
  void showEvent(QShowEvent *) override;
  void mouseMoveEvent(QMouseEvent *) override;
  void mousePressEvent(QMouseEvent *) override;
  void enterEvent(QEnterEvent *) override;
  void leaveEvent(QEvent *) override;

private:
  static void draw(void *data, uint32_t width, uint32_t height);
  static void drawSolidRect(float x, float y, float width, float height,
                            uint32_t canvasWidth, uint32_t canvasHeight,
                            const vec4 &color);
  void drawComparison(uint32_t canvasWidth, uint32_t canvasHeight,
                      int x, int y, int outputWidth, int outputHeight,
                      uint32_t sourceWidth, uint32_t sourceHeight);
  bool ensureCompareResources();
  void destroyCompareResources();
  void drawMagnifier(uint32_t width, uint32_t height);
  void createDisplay();
  void destroyDisplay();

  obs_display_t *display_ = nullptr;
  obs_source_t *source_ = nullptr;
  std::atomic<bool> pickMode_{false};
  std::atomic<bool> selected_{false};
  std::atomic<bool> compareMode_{false};
  std::array<std::atomic<float>, 7> compareSettings_{};
  gs_texrender_t *compareRender_ = nullptr;
  gs_effect_t *compareEffect_ = nullptr;
  std::atomic<float> hoverX_{-1.0f};
  std::atomic<float> hoverY_{-1.0f};
  std::function<void(const QPointF &)> clickHandler_;
  std::function<void(const QPoint &)> contextMenuHandler_;
  QWidget *emptyHint_ = nullptr;
};
