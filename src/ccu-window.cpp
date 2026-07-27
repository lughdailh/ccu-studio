#include "ccu-window.hpp"
#include "obs-display-widget.hpp"
#include "scope-data.hpp"
#include "scope-widget.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QMessageBox>
#include <QMenu>
#include <QPointer>
#include <QPainter>
#include <QPushButton>
#include <QGuiApplication>
#include <QResizeEvent>
#include <QScreen>
#include <QSlider>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWindow>

#ifdef __APPLE__
#include "ccu-window-mac.hpp"
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>

namespace {
QPointer<CcuWindow> activeWindow;
constexpr const char *filterId = "ccu_obs_color_filter";
constexpr const char *filterName = "CCU OBS";
constexpr const char *keys[] = {"red_gain",  "green_gain", "blue_gain",
                                "brightness", "contrast",   "gamma_value",
                                "saturation"};

class PreviewShell final : public QFrame {
public:
  explicit PreviewShell(QWidget *parent = nullptr) : QFrame(parent) {
    setMinimumSize(1, 1);
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  }
};

class PreviewContainer final : public QWidget {
public:
  explicit PreviewContainer(QWidget *parent = nullptr) : QWidget(parent) {}

  void setPreferredSize(const QSize &size) {
    if (preferredSize_ == size)
      return;
    preferredSize_ = size;
    updateGeometry();
  }

  QSize sizeHint() const override { return preferredSize_; }
  QSize minimumSizeHint() const override { return {1, 1}; }

private:
  QSize preferredSize_{1, 1};
};

int previewShellHeightForWidth(int width) {
  return static_cast<int>(std::lround(std::max(1, width) * 9.0 / 16.0));
}

QIcon whiteModuleIcon(const char *fileName) {
  char *path = obs_module_file(fileName);
  QPixmap pixels(path ? QString::fromUtf8(path) : QString());
  bfree(path);
  if (pixels.isNull())
    return {};
  QPainter painter(&pixels);
  painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
  painter.fillRect(pixels.rect(), Qt::white);
  painter.end();
  return QIcon(pixels);
}

QString roundButtonStyle() {
  return QStringLiteral(
      "QToolButton { background: #192a63; color: white; border: 1px solid "
      "#263b7d; border-radius: 28px; padding: 8px; }"
      "QToolButton:hover { background: #243a82; }"
      "QToolButton:checked { background: #304b9b; border-color: #d9ad32; }"
      "QToolButton:pressed { background: #101d49; }");
}

struct SliderSpec {
  const char *label;
  int minimum;
  int maximum;
  int neutral;
};
constexpr SliderSpec specs[] = {
    {"SliderRed", 50, 200, 100},        {"SliderGreen", 50, 200, 100},
    {"SliderBlue", 50, 200, 100},       {"SliderBrightness", -100, 100, 0},
    {"SliderContrast", 0, 200, 100},    {"SliderGamma", 20, 300, 100},
    {"SliderSaturation", 0, 200, 100},
};

bool enumerateVideoSources(void *data, obs_source_t *source) {
  auto *names = static_cast<QStringList *>(data);
  if ((obs_source_get_output_flags(source) & OBS_SOURCE_VIDEO) &&
      obs_source_get_type(source) == OBS_SOURCE_TYPE_INPUT)
    names->push_back(QString::fromUtf8(obs_source_get_name(source)));
  return true;
}

struct CapturedFrame {
  std::vector<std::uint8_t> rgba;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::uint32_t stride = 0;

  bool valid() const { return !rgba.empty() && width && height && stride; }
};

CapturedFrame captureSourceFrame(obs_source_t *source,
                                 std::uint32_t maximumWidth) {
  CapturedFrame result;
  if (!source)
    return result;
  const std::uint32_t sourceWidth = obs_source_get_width(source);
  const std::uint32_t sourceHeight = obs_source_get_height(source);
  if (!sourceWidth || !sourceHeight)
    return result;

  result.width = std::min(sourceWidth, maximumWidth);
  result.height = std::max(
      1u, static_cast<std::uint32_t>(std::lround(
              result.width * (double(sourceHeight) / sourceWidth))));
  result.stride = result.width * 4;

  obs_enter_graphics();
  gs_texrender_t *render = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
  gs_stagesurf_t *stage =
      gs_stagesurface_create(result.width, result.height, GS_RGBA);
  if (render && stage &&
      gs_texrender_begin_with_color_space(render, result.width, result.height,
                                          GS_CS_SRGB)) {
    vec4 clear{};
    gs_clear(GS_CLEAR_COLOR, &clear, 0.0f, 0);
    gs_ortho(0.0f, static_cast<float>(sourceWidth), 0.0f,
             static_cast<float>(sourceHeight), -100.0f, 100.0f);
    gs_set_viewport(0, 0, result.width, result.height);
    obs_source_video_render(source);
    gs_texrender_end(render);
    gs_stage_texture(stage, gs_texrender_get_texture(render));
    gs_flush();

    std::uint8_t *pixels = nullptr;
    std::uint32_t sourceStride = 0;
    if (gs_stagesurface_map(stage, &pixels, &sourceStride)) {
      result.rgba.resize(result.stride * result.height);
      for (std::uint32_t y = 0; y < result.height; ++y)
        std::memcpy(result.rgba.data() + y * result.stride,
                    pixels + y * sourceStride, result.stride);
      gs_stagesurface_unmap(stage);
    }
  }
  gs_stagesurface_destroy(stage);
  gs_texrender_destroy(render);
  obs_leave_graphics();
  if (!result.valid())
    return {};
  return result;
}

double settingValue(int index, int sliderValue) {
  if (index == 3)
    return sliderValue / 100.0;
  return sliderValue / 100.0;
}

int sliderValue(int index, double value) {
  (void)index;
  return static_cast<int>(std::lround(value * 100.0));
}

bool captureSample(obs_source_t *source, const QPointF &normalized,
                   std::array<double, 3> &rgb) {
  const CapturedFrame frame = captureSourceFrame(source, 960);
  if (!frame.valid())
    return false;
  const int centerX =
      std::clamp(static_cast<int>(normalized.x() * (frame.width - 1)), 0,
                 int(frame.width - 1));
  const int centerY =
      std::clamp(static_cast<int>(normalized.y() * (frame.height - 1)), 0,
                 int(frame.height - 1));
  double sums[3]{};
  int count = 0;
  for (int y = std::max(0, centerY - 3);
       y <= std::min(int(frame.height - 1), centerY + 3); ++y) {
    for (int x = std::max(0, centerX - 3);
         x <= std::min(int(frame.width - 1), centerX + 3); ++x) {
      const std::uint8_t *pixel =
          frame.rgba.data() + y * frame.stride + x * 4;
      sums[0] += pixel[0];
      sums[1] += pixel[1];
      sums[2] += pixel[2];
      ++count;
    }
  }
  if (!count)
    return false;
  for (int i = 0; i < 3; ++i)
    rgb[i] = sums[i] / (255.0 * count);
  return true;
}
} // namespace

CcuWindow::CcuWindow(QWidget *parent) : QDialog(parent) {
  setWindowTitle(QString::fromUtf8(obs_module_text("WindowTitle"))
                     .arg(QString::fromLatin1(CCU_VERSION)));
  setWindowFlag(Qt::Window, true);
  setModal(false);

  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(19, 21, 19, 18);
  rootLayout->setSpacing(22);

  auto *title = new QLabel(this);
  title->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  title->setFixedSize(290, 49);
  title->setContentsMargins(9, 0, 0, 0);
  char *titlePath = obs_module_file("CCUOBS_TTL.png");
  const QPixmap titlePixels(titlePath ? QString::fromUtf8(titlePath)
                                      : QString());
  bfree(titlePath);
  if (!titlePixels.isNull())
    title->setPixmap(titlePixels.scaled(
        281, 49, Qt::KeepAspectRatio, Qt::SmoothTransformation));
  title->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  rootLayout->addWidget(title, 0, Qt::AlignLeft);

  auto *mainLayout = new QHBoxLayout;
  mainLayout->setSpacing(12);
  rootLayout->addLayout(mainLayout, 1);

  leftPanel_ = new QWidget(this);
  auto *leftPanel = leftPanel_;
  leftLayout_ = new QVBoxLayout(leftPanel);
  auto *leftLayout = leftLayout_;
  leftLayout->setContentsMargins(0, 0, 0, 0);
  leftLayout->setSpacing(8);

  previewContainer_ = new PreviewContainer(leftPanel);
  previewContainer_->setMinimumSize(1, 1);
  previewContainer_->setSizePolicy(QSizePolicy::Expanding,
                                   QSizePolicy::Expanding);

  for (int i = 0; i < 4; ++i) {
    Channel &channel = channels_[i];
    channel.frame = new QFrame(previewContainer_);
    auto *layout = new QVBoxLayout(channel.frame);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    channel.sourceBox = new QComboBox(leftPanel);
    channel.sourceBox->hide();
    channel.previewShell = new PreviewShell(channel.frame);
    channel.previewShell->setObjectName(
        QStringLiteral("previewShell%1").arg(i + 1));
    auto *previewLayout = new QVBoxLayout(channel.previewShell);
    previewLayout->setContentsMargins(0, 0, 0, 0);
    previewLayout->setSpacing(0);
    channel.preview = new ObsDisplayWidget(channel.previewShell);
    previewLayout->addWidget(channel.preview);
    layout->addWidget(channel.previewShell);
    connect(channel.sourceBox, &QComboBox::currentIndexChanged, this,
            [this, i](int) { chooseSource(i); });
    channel.preview->setClickHandler([this, i](const QPointF &position) {
      if (pickerActive_ && i == selected_)
        sampleWhite(i, position);
      else if (!pickerActive_)
        selectChannel(i);
    });
    channel.preview->setContextMenuHandler(
        [this, i](const QPoint &position) { showSourceMenu(i, position); });
    channel.preview->setToolTip(
        QString::fromUtf8(obs_module_text("SourceMenuTooltip")));
  }

  leftLayout->addWidget(previewContainer_, 1);

  auto *cameraControlGroup = new QWidget(this);
  cameraControlGroup->setSizePolicy(QSizePolicy::Preferred,
                                    QSizePolicy::Fixed);
  auto *selectors = new QVBoxLayout(cameraControlGroup);
  selectors->setContentsMargins(0, 0, 0, 0);
  selectors->setSpacing(4);
  auto *activeCameraLabel =
      new QLabel(QString::fromUtf8(obs_module_text("ActiveCameraLabel")),
                 cameraControlGroup);
  activeCameraLabel->setAlignment(Qt::AlignHCenter);
  selectors->addWidget(activeCameraLabel);
  auto *cameraButtons = new QHBoxLayout;
  cameraButtons->setSpacing(18);
  cameraButtons->addStretch();
  for (int i = 0; i < 4; ++i) {
    selectButtons_[i] = new QToolButton(cameraControlGroup);
    selectButtons_[i]->setText(QString::number(i + 1));
    selectButtons_[i]->setToolButtonStyle(Qt::ToolButtonTextOnly);
    selectButtons_[i]->setCheckable(true);
    selectButtons_[i]->setFixedSize(56, 56);
    selectButtons_[i]->setStyleSheet(
        roundButtonStyle() +
        QStringLiteral("QToolButton { font-size: 16px; padding: 0; }"));
    cameraButtons->addWidget(selectButtons_[i]);
    connect(selectButtons_[i], &QToolButton::clicked, this,
            [this, i] { selectChannel(i); });
  }
  cameraButtons->addStretch();
  selectors->addLayout(cameraButtons);
  leftLayout->addWidget(cameraControlGroup);
  leftLayout->addSpacing(15);

  auto *tools = new QHBoxLayout;
  tools->setSpacing(12);
  tools->addStretch();
  auto makeToolButton = [this](const char *textKey, const char *iconFile) {
    auto *button = new QToolButton(this);
    button->setText(QString::fromUtf8(obs_module_text(textKey)));
    button->setIcon(whiteModuleIcon(iconFile));
    button->setIconSize({28, 28});
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setFixedSize(56, 56);
    button->setCheckable(true);
    button->setStyleSheet(roundButtonStyle());
    return button;
  };
  auto toolWithLabel = [this](QToolButton *button) {
    auto *container = new QWidget(this);
    container->setFixedWidth(100);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    layout->setAlignment(Qt::AlignHCenter);
    auto *label = new QLabel(button->text(), container);
    label->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    label->setStyleSheet(QStringLiteral("color: #dddddd;"));
    layout->addWidget(button, 0, Qt::AlignHCenter);
    layout->addWidget(label, 0, Qt::AlignHCenter);
    return container;
  };
  pickerButton_ =
      makeToolButton("PickerButton", "icons/comptegotes.png");
  pickerButton_->setCheckable(true);
  zoomButton_ = makeToolButton("ZoomButton", "icons/lopa.png");
  zoomButton_->setCheckable(true);
  freezeButton_ = makeToolButton("FreezeButton", "icons/freeze.png");
  freezeButton_->setCheckable(true);
  freezeButton_->setToolTip(
      QString::fromUtf8(obs_module_text("FreezeTooltip")));
  compareButton_ = makeToolButton("CompareButton", "icons/original.png");
  compareButton_->setCheckable(true);
  compareButton_->setToolTip(
      QString::fromUtf8(obs_module_text("CompareTooltip")));
  auto *reset = makeToolButton("ResetButton", "icons/restablir.png");
  reset->setCheckable(false);
  tools->addWidget(toolWithLabel(pickerButton_));
  tools->addWidget(toolWithLabel(zoomButton_));
  tools->addWidget(toolWithLabel(freezeButton_));
  tools->addWidget(toolWithLabel(compareButton_));
  tools->addWidget(toolWithLabel(reset));
  tools->addStretch();
  leftLayout->addLayout(tools);
  connect(pickerButton_, &QToolButton::clicked, this, [this] { togglePicker(); });
  connect(zoomButton_, &QToolButton::clicked, this, [this] { toggleZoom(); });
  connect(freezeButton_, &QToolButton::clicked, this,
          [this] { toggleScopeFreeze(); });
  connect(compareButton_, &QToolButton::clicked, this,
          [this] { toggleCompare(); });
  connect(reset, &QToolButton::clicked, this, [this] { resetControls(); });

  auto *rightPanel = new QFrame(this);
  rightPanel_ = rightPanel;
  rightPanel->setFrameShape(QFrame::StyledPanel);
  rightPanel->setMinimumWidth(330);
  rightPanel->setMaximumWidth(520);
  auto *rightLayout = new QVBoxLayout(rightPanel);
  rightLayout->setContentsMargins(10, 10, 10, 10);
  rightLayout->setSpacing(8);
  status_ = new QLabel(
      QString::fromUtf8(obs_module_text("StatusSelectSource")), rightPanel);
  status_->setWordWrap(true);
  // Status messages remain available to the interaction logic and assistive
  // tools, but the visual composition follows the CCU reference: the scope
  // starts level with the preview mosaic rather than under a status banner.
  status_->hide();

  auto *scopeTabs = new QTabWidget(rightPanel);
  scopeTabs_ = scopeTabs;
  histogram_ = new ScopeWidget(ScopeWidget::Type::Histogram, scopeTabs);
  waveform_ = new ScopeWidget(ScopeWidget::Type::Waveform, scopeTabs);
  vectorscope_ = new ScopeWidget(ScopeWidget::Type::Vectorscope, scopeTabs);
  scopeTabs->addTab(histogram_, QString::fromUtf8(obs_module_text("TabHistogram")));
  scopeTabs->addTab(waveform_, QString::fromUtf8(obs_module_text("TabWaveform")));
  scopeTabs->addTab(vectorscope_,
                    QString::fromUtf8(obs_module_text("TabVectorscope")));
  scopeTabs->setTabPosition(QTabWidget::South);
  scopeTabs->setMinimumHeight(300);
  scopeTabs->setMaximumHeight(465);
  scopeTabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  rightLayout->addWidget(scopeTabs);
  rightLayout->addStretch(1);

  auto *generalGroup = new QGroupBox(
      QString::fromUtf8(obs_module_text("GroupLevels")), rightPanel);
  generalGroup->setFixedHeight(188);
  auto *generalControls = new QGridLayout(generalGroup);
  auto *rgbGroup = new QGroupBox(
      QString::fromUtf8(obs_module_text("GroupLevelsRGB")), rightPanel);
  rgbGroup->setFixedHeight(161);
  auto *rgbControls = new QGridLayout(rgbGroup);

  QSlider **sliders[] = {&red_, &green_, &blue_, &brightness_,
                         &contrast_, &gamma_, &saturation_};
  auto addControl = [&](QGridLayout *layout, int row, int index) {
    auto *label =
        new QLabel(QString::fromUtf8(obs_module_text(specs[index].label)),
                   layout->parentWidget());
    *sliders[index] =
        new QSlider(Qt::Horizontal, layout->parentWidget());
    (*sliders[index])->setRange(specs[index].minimum, specs[index].maximum);
    (*sliders[index])->setValue(specs[index].neutral);
    auto *value = new QLabel(layout->parentWidget());
    value->setMinimumWidth(48);
    value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    layout->addWidget(label, row, 0);
    layout->addWidget(*sliders[index], row, 1);
    layout->addWidget(value, row, 2);
    layout->setColumnStretch(1, 1);
    connect(*sliders[index], &QSlider::valueChanged, this,
            [this, value, index](int number) {
              value->setText(
                  QString::number(settingValue(index, number), 'f', 2));
              applyControls();
            });
    value->setText(
        QString::number(settingValue(index, specs[index].neutral), 'f', 2));
  };
  addControl(generalControls, 0, 3);
  addControl(generalControls, 1, 4);
  addControl(generalControls, 2, 5);
  addControl(generalControls, 3, 6);
  addControl(rgbControls, 0, 0);
  addControl(rgbControls, 1, 1);
  addControl(rgbControls, 2, 2);
  rightLayout->addWidget(generalGroup);
  rightLayout->addWidget(rgbGroup);

  mainLayout->addWidget(leftPanel, 31);
  mainLayout->addWidget(rightPanel, 10);

  refreshSources();
  loadAssignments();
  selectChannel(0);

  scopeTimer_ = new QTimer(this);
  scopeTimer_->setInterval(125);
  connect(scopeTimer_, &QTimer::timeout, this, [this] { updateScopes(); });
  scopeTimer_->start();

  QTimer::singleShot(0, this, [this] {
    QScreen *targetScreen = nullptr;
    if (QWidget *mainWindow =
            static_cast<QWidget *>(obs_frontend_get_main_window())) {
      targetScreen = mainWindow->screen();
    }
    if (!targetScreen)
      targetScreen = QGuiApplication::primaryScreen();
    if (!targetScreen)
      return;
    const QRect available = targetScreen->availableGeometry();
    // Work in Qt logical pixels so the same limits behave consistently on
    // macOS Retina screens and Windows displays with DPI scaling.  A 1080p
    // desktop therefore gets a comfortably fitting window instead of one
    // whose controls crowd the taskbar or OBS chrome.
    constexpr int maximumInitialWidth = 1650;
    constexpr int maximumInitialHeight = 990;
    const QSize maximumAvailableSize(
        static_cast<int>(std::lround(available.width() * 0.85)),
        static_cast<int>(std::lround(available.height() * 0.85)));
    const QSize targetSize =
        QSize(maximumInitialWidth, maximumInitialHeight)
            .scaled(maximumAvailableSize, Qt::KeepAspectRatio);
    resize(targetSize);
    applyWindowAspectRatio();
    move(available.center() - QPoint(width() / 2, height() / 2));
  });
}

CcuWindow::~CcuWindow() {
  saveAssignments();
  for (Channel &channel : channels_)
    if (channel.source)
      obs_source_release(channel.source);
}

void CcuWindow::applyWindowAspectRatio() {
#ifdef __APPLE__
  // Reapply after show() as well as after construction: an NSWindow is not
  // guaranteed to exist when the dialog's first queued size calculation
  // runs, and setContentAspectRatio on a detached NSView has no effect.
  ccuSetWindowAspectRatioMac(reinterpret_cast<void *>(winId()), 5.0 / 3.0);
#endif
}

void CcuWindow::refreshSources() {
  QStringList names;
  obs_enum_sources(enumerateVideoSources, &names);
  names.sort(Qt::CaseInsensitive);
  for (Channel &channel : channels_) {
    const QString current = channel.sourceBox->currentText();
    channel.sourceBox->blockSignals(true);
    channel.sourceBox->clear();
    channel.sourceBox->addItem(QString::fromUtf8(obs_module_text("NoSourceItem")));
    channel.sourceBox->addItems(names);
    const int match = channel.sourceBox->findText(current);
    if (match >= 0)
      channel.sourceBox->setCurrentIndex(match);
    channel.sourceBox->blockSignals(false);
  }
}

void CcuWindow::selectChannel(int index) {
  selected_ = std::clamp(index, 0, 3);
  for (int i = 0; i < 4; ++i) {
    const bool active = i == selected_;
    selectButtons_[i]->setChecked(active);
    channels_[i].previewShell->setStyleSheet(QString());
    channels_[i].preview->setSelected(active);
    channels_[i].preview->setPickMode(pickerActive_ && active);
  }
  if (zoomed_)
    relayoutChannels();
  updateSelectedUi();
  updateComparePreview();
}

void CcuWindow::chooseSource(int index) {
  Channel &channel = channels_[index];
  if (channel.source) {
    obs_source_release(channel.source);
    channel.source = nullptr;
  }
  if (channel.sourceBox->currentIndex() > 0)
    channel.source = obs_get_source_by_name(
        channel.sourceBox->currentText().toUtf8().constData());
  channel.preview->setSource(channel.source);
  if (channel.source) {
    channel.preview->setToolTip(
        QString::fromUtf8(obs_module_text("SourceAssignedTooltip"))
            .arg(channel.sourceBox->currentText())
            .arg(obs_source_get_width(channel.source))
            .arg(obs_source_get_height(channel.source)));
  } else {
    channel.preview->setToolTip(
        QString::fromUtf8(obs_module_text("SourceMenuTooltip")));
  }
  if (channel.source)
    ensureFilter(channel.source);
  saveAssignments();
  if (index == selected_)
    updateSelectedUi();
}

void CcuWindow::showSourceMenu(int index, const QPoint &globalPosition) {
  selectChannel(index);
  refreshSources();

  QMenu menu(this);
  menu.setTitle(QString::fromUtf8(obs_module_text("SourceMenuTitle")));
  Channel &channel = channels_[index];
  for (int item = 0; item < channel.sourceBox->count(); ++item) {
    QAction *action = menu.addAction(channel.sourceBox->itemText(item));
    action->setCheckable(true);
    action->setChecked(item == channel.sourceBox->currentIndex());
    connect(action, &QAction::triggered, this, [this, index, item] {
      channels_[index].sourceBox->setCurrentIndex(item);
    });
  }
  menu.exec(globalPosition);
}

obs_source_t *CcuWindow::ensureFilter(obs_source_t *source) {
  if (!source)
    return nullptr;
  if (obs_source_t *existing =
          obs_source_get_filter_by_name(source, filterName))
    return existing;
  obs_data_t *settings = obs_data_create();
  obs_source_t *filter =
      obs_source_create_private(filterId, filterName, settings);
  obs_data_release(settings);
  if (filter)
    obs_source_filter_add(source, filter);
  return filter;
}

void CcuWindow::updateSelectedUi() {
  updating_ = true;
  obs_source_t *filter = ensureFilter(channels_[selected_].source);
  QSlider *sliders[] = {red_, green_, blue_, brightness_,
                        contrast_, gamma_, saturation_};
  if (filter) {
    obs_data_t *settings = obs_source_get_settings(filter);
    for (int i = 0; i < 7; ++i)
      sliders[i]->setValue(sliderValue(i, obs_data_get_double(settings, keys[i])));
    obs_data_release(settings);
    obs_source_release(filter);
    status_->setText(
        QString::fromUtf8(obs_module_text("StatusEditingCamera"))
            .arg(selected_ + 1)
            .arg(channels_[selected_].sourceBox->currentText()));
  } else {
    for (int i = 0; i < 7; ++i)
      sliders[i]->setValue(specs[i].neutral);
    status_->setText(QString::fromUtf8(obs_module_text("StatusNoSource"))
                          .arg(selected_ + 1));
  }
  updating_ = false;
}

void CcuWindow::applyControls() {
  if (updating_)
    return;
  obs_source_t *filter = ensureFilter(channels_[selected_].source);
  if (!filter)
    return;
  QSlider *sliders[] = {red_, green_, blue_, brightness_,
                        contrast_, gamma_, saturation_};
  obs_data_t *settings = obs_source_get_settings(filter);
  for (int i = 0; i < 7; ++i)
    obs_data_set_double(settings, keys[i], settingValue(i, sliders[i]->value()));
  obs_source_update(filter, settings);
  obs_data_release(settings);
  obs_source_release(filter);
  updateComparePreview();
}

void CcuWindow::resetControls() {
  updating_ = true;
  QSlider *sliders[] = {red_, green_, blue_, brightness_,
                        contrast_, gamma_, saturation_};
  for (int i = 0; i < 7; ++i)
    sliders[i]->setValue(specs[i].neutral);
  updating_ = false;
  applyControls();
  status_->setText(QString::fromUtf8(obs_module_text("StatusCorrectionReset")));
}

void CcuWindow::togglePicker() {
  pickerActive_ = pickerButton_->isChecked();
  for (int i = 0; i < 4; ++i)
    channels_[i].preview->setPickMode(pickerActive_ && i == selected_);
  status_->setText(QString::fromUtf8(obs_module_text(
      pickerActive_ ? "StatusPickerActive" : "StatusPickerInactive")));
}

void CcuWindow::toggleZoom() {
  zoomed_ = zoomButton_->isChecked();
  relayoutChannels();
  status_->setText(QString::fromUtf8(obs_module_text(
      zoomed_ ? "StatusZoomActive" : "StatusZoomInactive")));
}

void CcuWindow::toggleScopeFreeze() {
  if (!freezeButton_)
    return;

  if (freezeButton_->isChecked()) {
    // Capture on demand so a click immediately after changing camera cannot
    // accidentally freeze the previous camera's last timer sample.
    updateScopes();
    if (lastScopeData_.empty() || lastScopeChannel_ != selected_) {
      freezeButton_->setChecked(false);
      status_->setText(
          QString::fromUtf8(obs_module_text("StatusFreezeUnavailable")));
      return;
    }
    frozenScopeChannel_ = selected_;
    histogram_->setReferenceData(lastScopeData_);
    waveform_->setReferenceData(lastScopeData_);
    vectorscope_->setReferenceData(lastScopeData_);
    status_->setText(
        QString::fromUtf8(obs_module_text("StatusFreezeActive"))
            .arg(frozenScopeChannel_ + 1));
  } else {
    frozenScopeChannel_ = -1;
    histogram_->clearReference();
    waveform_->clearReference();
    vectorscope_->clearReference();
    status_->setText(
        QString::fromUtf8(obs_module_text("StatusFreezeCleared")));
  }
}

void CcuWindow::toggleCompare() {
  updateComparePreview();
  status_->setText(QString::fromUtf8(obs_module_text(
      compareButton_->isChecked() ? "StatusCompareActive"
                                  : "StatusCompareInactive")));
}

void CcuWindow::updateComparePreview() {
  std::array<float, 7> settings{1, 1, 1, 0, 1, 1, 1};
  QSlider *sliders[] = {red_, green_, blue_, brightness_,
                        contrast_, gamma_, saturation_};
  for (int i = 0; i < 7; ++i)
    if (sliders[i])
      settings[i] = static_cast<float>(
          settingValue(i, sliders[i]->value()));
  for (int i = 0; i < 4; ++i)
    channels_[i].preview->setCompareMode(
        compareButton_ && compareButton_->isChecked() && i == selected_,
        settings);
}

void CcuWindow::relayoutChannels() {
  if (!previewContainer_)
    return;
  for (int i = 0; i < 4; ++i) {
    channels_[i].frame->setVisible(!zoomed_ || i == selected_);
  }
  updatePreviewSizes();
  // The shrinking preview shell can leave stale pixels behind in the area
  // it no longer covers (the source combo box briefly appeared to be
  // duplicated there) - force a full repaint of the affected area rather
  // than relying on Qt's normal damage tracking to notice.
  leftPanel_->update();
}

void CcuWindow::updatePreviewSizes() {
  previewResizePending_ = false;
  if (!leftPanel_ || !previewContainer_)
    return;
  // QGridLayout doesn't reliably honor heightForWidth, so the 16:9 preview
  // aspect ratio is driven explicitly from the actual column width instead
  // of letting each shell fight the layout reactively in its own
  // resizeEvent (that caused the preview to render at a stale, mismatched
  // size).
  const QMargins panelMargins = leftLayout_->contentsMargins();
  int fixedControlsHeight = panelMargins.top() + panelMargins.bottom();
  for (int i = 0; i < leftLayout_->count(); ++i) {
    QLayoutItem *item = leftLayout_->itemAt(i);
    if (item && item->widget() != previewContainer_)
      fixedControlsHeight += item->sizeHint().height();
  }
  fixedControlsHeight +=
      std::max(0, leftLayout_->count() - 1) * leftLayout_->spacing();

  const int availableMosaicHeight =
      std::max(1, leftPanel_->height() - fixedControlsHeight);
  const int availableMosaicWidth =
      std::max(1, leftPanel_->width() - panelMargins.left() -
                      panelMargins.right());
  constexpr int horizontalGap = 1;
  constexpr int verticalGap = 1;
  const int widthLimitedColumn =
      std::max(1, (availableMosaicWidth - horizontalGap) / 2);
  const int widthLimitedRowHeight =
      previewShellHeightForWidth(widthLimitedColumn);
  const int heightLimitedRowHeight =
      std::max(1, (availableMosaicHeight - verticalGap) / 2);
  const int rowHeight =
      std::min(widthLimitedRowHeight, heightLimitedRowHeight);
  const int columnWidth = std::max(
      1, static_cast<int>(std::lround(rowHeight * (16.0 / 9.0))));
  // The zoomed preview deliberately keeps the exact same outer rectangle as
  // the 2×2 mosaic. Changing the container's size here used to make the
  // camera/tool controls jump and made the enlarged image start and end at
  // different coordinates than the four-camera view.
  const int mosaicWidth = columnWidth * 2 + horizontalGap;
  const int mosaicHeight = rowHeight * 2 + verticalGap;
  static_cast<PreviewContainer *>(previewContainer_)
      ->setPreferredSize({mosaicWidth, mosaicHeight});
  leftLayout_->setAlignment(previewContainer_, Qt::AlignHCenter);
  previewContainer_->setMinimumSize(1, 1);
  previewContainer_->setMaximumSize(mosaicWidth, mosaicHeight);
  previewContainer_->setSizePolicy(QSizePolicy::Expanding,
                                   QSizePolicy::Expanding);

  // Applying a new maximum changes the space QVBoxLayout assigns to the
  // container. Settle that geometry before placing the four unmanaged
  // frames; otherwise they can be laid out against the old 1×1 startup
  // rectangle and remain invisible until another window resize.
  leftLayout_->invalidate();
  leftLayout_->activate();

  // These frames deliberately use direct geometry inside one stable parent.
  // A QGridLayout turned each requested preview size into a dialog minimum
  // and distributed spare height between rows. Direct placement keeps the
  // one-pixel joins exact without reparenting the native OBS views.
  const QRect area = previewContainer_->rect();
  if (zoomed_) {
    channels_[selected_].frame->setGeometry(area);
  } else {
    const int widthLimit = std::max(1, (area.width() - horizontalGap) / 2);
    const int heightLimit = std::max(1, (area.height() - verticalGap) / 2);
    const int cellWidth = std::max(
        1, std::min(widthLimit, static_cast<int>(
                                    std::floor(heightLimit * (16.0 / 9.0)))));
    const int cellHeight = std::max(
        1, static_cast<int>(std::floor(cellWidth * (9.0 / 16.0))));
    const int occupiedWidth = cellWidth * 2 + horizontalGap;
    const int occupiedHeight = cellHeight * 2 + verticalGap;
    const int offsetX = std::max(0, (area.width() - occupiedWidth) / 2);
    const int offsetY = std::max(0, (area.height() - occupiedHeight) / 2);
    for (int i = 0; i < 4; ++i) {
      const int row = i / 2;
      const int column = i % 2;
      channels_[i].frame->setGeometry(
          offsetX + column * (cellWidth + horizontalGap),
          offsetY + row * (cellHeight + verticalGap), cellWidth, cellHeight);
    }
  }
}

void CcuWindow::resizeEvent(QResizeEvent *event) {
  QDialog::resizeEvent(event);
  if (scopeTabs_ && rightPanel_) {
    const int scopeHeight =
        std::clamp(static_cast<int>(std::lround(rightPanel_->height() * 0.53)),
                   300, 465);
    scopeTabs_->setFixedHeight(scopeHeight);
  }
  // At this point the dialog has its new size but its child layouts and
  // native OBS views may still have their old geometry. One queued update
  // per event-loop pass lets Qt settle the layout first, then the child
  // resizeEvents call obs_display_resize with the final dimensions.
  if (!previewResizePending_) {
    previewResizePending_ = true;
    QTimer::singleShot(0, this, [this] { updatePreviewSizes(); });
  }
}

void CcuWindow::updateScopes() {
  if (!isVisible() || pickerActive_)
    return;
  obs_source_t *source = channels_[selected_].source;
  if (!source) {
    lastScopeData_ = ScopeData{};
    lastScopeChannel_ = -1;
    histogram_->clear();
    waveform_->clear();
    vectorscope_->clear();
    return;
  }
  const CapturedFrame frame = captureSourceFrame(source, 480);
  if (!frame.valid())
    return;
  const ScopeData scope =
      analyzeScopeFrame(frame.rgba.data(), frame.width, frame.height,
                        frame.stride);
  lastScopeData_ = scope;
  lastScopeChannel_ = selected_;
  histogram_->setScopeData(scope);
  waveform_->setScopeData(scope);
  vectorscope_->setScopeData(scope);
}

void CcuWindow::sampleWhite(int channelIndex, const QPointF &normalized) {
  // The eyedropper is deliberately locked to the selected camera. Keep this
  // guard even though inactive previews do not forward picker clicks: it
  // prevents any future input path from applying a sample to the wrong source.
  if (!pickerActive_ || channelIndex != selected_)
    return;
  obs_source_t *source = channels_[channelIndex].source;
  obs_source_t *filter = ensureFilter(source);
  if (!source || !filter)
    return;
  const bool enabled = obs_source_enabled(filter);
  obs_source_set_enabled(filter, false);
  std::array<double, 3> sample{};
  const bool captured = captureSample(source, normalized, sample);
  obs_source_set_enabled(filter, enabled);
  if (captured) {
    const double maximum = std::max({sample[0], sample[1], sample[2]});
    if (maximum > 0.01) {
      updating_ = true;
      red_->setValue(static_cast<int>(std::lround(
          std::clamp(maximum / sample[0], 0.5, 2.0) * 100.0)));
      green_->setValue(static_cast<int>(std::lround(
          std::clamp(maximum / sample[1], 0.5, 2.0) * 100.0)));
      blue_->setValue(static_cast<int>(std::lround(
          std::clamp(maximum / sample[2], 0.5, 2.0) * 100.0)));
      updating_ = false;
      applyControls();
      status_->setText(
          QString::fromUtf8(obs_module_text("StatusSampleRGB"))
              .arg(sample[0], 0, 'f', 3)
              .arg(sample[1], 0, 'f', 3)
              .arg(sample[2], 0, 'f', 3));
    }
  } else {
    status_->setText(
        QString::fromUtf8(obs_module_text("StatusCaptureFailed")));
  }
  obs_source_release(filter);
  pickerButton_->setChecked(false);
  togglePicker();
}

void CcuWindow::loadAssignments() {
  char *path = obs_module_config_path("assignments.json");
  QFile file(QString::fromUtf8(path ? path : ""));
  bfree(path);
  if (!file.open(QIODevice::ReadOnly))
    return;
  const QJsonArray names = QJsonDocument::fromJson(file.readAll()).array();
  for (int i = 0; i < 4 && i < names.size(); ++i) {
    const int match = channels_[i].sourceBox->findText(names[i].toString());
    if (match >= 0)
      channels_[i].sourceBox->setCurrentIndex(match);
  }
}

void CcuWindow::saveAssignments() const {
  char *path = obs_module_config_path("assignments.json");
  if (!path)
    return;
  QFile file(QString::fromUtf8(path));
  bfree(path);
  QDir().mkpath(QFileInfo(file).absolutePath());
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return;
  QJsonArray names;
  for (const Channel &channel : channels_)
    names.push_back(channel.sourceBox->currentIndex() > 0
                        ? channel.sourceBox->currentText()
                        : QString());
  file.write(QJsonDocument(names).toJson(QJsonDocument::Compact));
}

void showCcuWindow() {
  if (!activeWindow) {
    activeWindow =
        new CcuWindow(static_cast<QWidget *>(obs_frontend_get_main_window()));
    activeWindow->setAttribute(Qt::WA_DeleteOnClose);
  }
  activeWindow->show();
  activeWindow->applyWindowAspectRatio();
  activeWindow->raise();
  activeWindow->activateWindow();
#ifdef __APPLE__
  // Qt's raise()/activateWindow() go through its own activation
  // abstraction, which on macOS can be a no-op while OBS's own window
  // still holds focus - the dialog would show() but stay stuck behind
  // OBS's main window with no obvious way to bring it forward again.
  // Cocoa's own activation call actually steals focus.
  ccuActivateWindowMac(reinterpret_cast<void *>(activeWindow->winId()));
#endif
  // On macOS in particular, raising/activating right after show() can be a
  // no-op if the native window isn't fully mapped yet, leaving the dialog
  // stuck behind OBS's main window. Repeating it on the next event loop
  // turn, once the window is actually up, makes it reliable.
  QTimer::singleShot(0, activeWindow, [] {
    if (!activeWindow)
      return;
    activeWindow->raise();
    activeWindow->activateWindow();
    if (QWindow *handle = activeWindow->windowHandle())
      handle->requestActivate();
#ifdef __APPLE__
    activeWindow->applyWindowAspectRatio();
    ccuActivateWindowMac(reinterpret_cast<void *>(activeWindow->winId()));
#endif
  });
}

void closeCcuWindow() {
  if (activeWindow)
    activeWindow->close();
  activeWindow = nullptr;
}
