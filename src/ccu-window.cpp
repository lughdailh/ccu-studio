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
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QGuiApplication>
#include <QResizeEvent>
#include <QScreen>
#include <QSlider>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>

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
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  }
};

int previewShellHeightForWidth(int width) {
  const int contentWidth = std::max(1, width - 6);
  return static_cast<int>(std::lround(contentWidth * 9.0 / 16.0)) + 6;
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

  auto *mainLayout = new QHBoxLayout(this);
  mainLayout->setSpacing(12);

  leftPanel_ = new QWidget(this);
  auto *leftPanel = leftPanel_;
  auto *leftLayout = new QVBoxLayout(leftPanel);
  leftLayout->setContentsMargins(0, 0, 0, 0);
  leftLayout->setSpacing(8);
  previewGrid_ = new QGridLayout;
  previewGrid_->setContentsMargins(0, 0, 0, 0);
  previewGrid_->setHorizontalSpacing(8);
  previewGrid_->setVerticalSpacing(8);
  previewGrid_->setColumnStretch(0, 1);
  previewGrid_->setColumnStretch(1, 1);
  previewGrid_->setRowStretch(0, 1);
  previewGrid_->setRowStretch(1, 1);

  for (int i = 0; i < 4; ++i) {
    Channel &channel = channels_[i];
    channel.frame = new QFrame(leftPanel);
    channel.frame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto *layout = new QVBoxLayout(channel.frame);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    channel.sourceBox = new QComboBox(channel.frame);
    channel.previewShell = new PreviewShell(channel.frame);
    channel.previewShell->setObjectName(
        QStringLiteral("previewShell%1").arg(i + 1));
    auto *previewLayout = new QVBoxLayout(channel.previewShell);
    previewLayout->setContentsMargins(3, 3, 3, 3);
    channel.preview = new ObsDisplayWidget(channel.previewShell);
    previewLayout->addWidget(channel.preview);
    auto *label = new QLabel(
        QString::fromUtf8(obs_module_text("CameraLabel")).arg(i + 1),
        channel.frame);
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(channel.sourceBox);
    layout->addWidget(channel.previewShell);
    layout->addWidget(label);
    previewGrid_->addWidget(channel.frame, i / 2, i % 2, Qt::AlignTop);
    connect(channel.sourceBox, &QComboBox::currentIndexChanged, this,
            [this, i](int) { chooseSource(i); });
    channel.preview->setClickHandler([this, i](const QPointF &position) {
      if (pickerActive_)
        sampleWhite(i, position);
      else
        selectChannel(i);
    });
  }
  leftLayout->addLayout(previewGrid_, 1);

  auto *selectors = new QHBoxLayout;
  selectors->addStretch();
  selectors->addWidget(
      new QLabel(QString::fromUtf8(obs_module_text("ActiveCameraLabel")), this));
  for (int i = 0; i < 4; ++i) {
    selectButtons_[i] = new QPushButton(QString::number(i + 1), this);
    selectButtons_[i]->setCheckable(true);
    selectButtons_[i]->setMinimumWidth(52);
    selectors->addWidget(selectButtons_[i]);
    connect(selectButtons_[i], &QPushButton::clicked, this,
            [this, i] { selectChannel(i); });
  }
  selectors->addStretch();
  leftLayout->addLayout(selectors);

  auto *tools = new QHBoxLayout;
  tools->addStretch();
  pickerButton_ =
      new QPushButton(QString::fromUtf8(obs_module_text("PickerButton")), this);
  pickerButton_->setCheckable(true);
  zoomButton_ =
      new QPushButton(QString::fromUtf8(obs_module_text("ZoomButton")), this);
  zoomButton_->setCheckable(true);
  auto *reset =
      new QPushButton(QString::fromUtf8(obs_module_text("ResetButton")), this);
  tools->addWidget(pickerButton_);
  tools->addWidget(zoomButton_);
  tools->addWidget(reset);
  tools->addStretch();
  leftLayout->addLayout(tools);
  connect(pickerButton_, &QPushButton::clicked, this, [this] { togglePicker(); });
  connect(zoomButton_, &QPushButton::clicked, this, [this] { toggleZoom(); });
  connect(reset, &QPushButton::clicked, this, [this] { resetControls(); });

  auto *rightPanel = new QFrame(this);
  rightPanel->setFrameShape(QFrame::StyledPanel);
  rightPanel->setMinimumWidth(330);
  rightPanel->setMaximumWidth(520);
  auto *rightLayout = new QVBoxLayout(rightPanel);
  rightLayout->setContentsMargins(10, 10, 10, 10);
  rightLayout->setSpacing(8);
  status_ = new QLabel(
      QString::fromUtf8(obs_module_text("StatusSelectSource")), rightPanel);
  status_->setWordWrap(true);
  status_->setMinimumHeight(38);
  rightLayout->addWidget(status_);

  auto *scopeTabs = new QTabWidget(rightPanel);
  histogram_ = new ScopeWidget(ScopeWidget::Type::Histogram, scopeTabs);
  waveform_ = new ScopeWidget(ScopeWidget::Type::Waveform, scopeTabs);
  vectorscope_ = new ScopeWidget(ScopeWidget::Type::Vectorscope, scopeTabs);
  scopeTabs->addTab(histogram_, QString::fromUtf8(obs_module_text("TabHistogram")));
  scopeTabs->addTab(waveform_, QString::fromUtf8(obs_module_text("TabWaveform")));
  scopeTabs->addTab(vectorscope_,
                    QString::fromUtf8(obs_module_text("TabVectorscope")));
  rightLayout->addWidget(scopeTabs, 1);

  auto *generalGroup = new QGroupBox(
      QString::fromUtf8(obs_module_text("GroupLevels")), rightPanel);
  auto *generalControls = new QGridLayout(generalGroup);
  auto *rgbGroup = new QGroupBox(
      QString::fromUtf8(obs_module_text("GroupLevelsRGB")), rightPanel);
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

  mainLayout->addWidget(leftPanel, 3);
  mainLayout->addWidget(rightPanel, 1);

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
    const QSize targetSize(
        static_cast<int>(std::lround(available.width() * 0.85)),
        static_cast<int>(std::lround(available.height() * 0.85)));
    resize(targetSize);
    move(available.center() - QPoint(width() / 2, height() / 2));
  });
}

CcuWindow::~CcuWindow() {
  saveAssignments();
  for (Channel &channel : channels_)
    if (channel.source)
      obs_source_release(channel.source);
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
    channels_[i].previewShell->setStyleSheet(
        active
            ? QStringLiteral(
                  "QFrame#previewShell%1 { border: 3px solid #e6b422; }")
                  .arg(i + 1)
            : QStringLiteral(
                  "QFrame#previewShell%1 { border: 3px solid transparent; }")
                  .arg(i + 1));
  }
  if (zoomed_)
    relayoutChannels();
  updateSelectedUi();
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
  if (channel.source)
    ensureFilter(channel.source);
  saveAssignments();
  if (index == selected_)
    updateSelectedUi();
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
  for (Channel &channel : channels_)
    channel.preview->setPickMode(pickerActive_);
  status_->setText(QString::fromUtf8(obs_module_text(
      pickerActive_ ? "StatusPickerActive" : "StatusPickerInactive")));
}

void CcuWindow::toggleZoom() {
  zoomed_ = zoomButton_->isChecked();
  relayoutChannels();
  status_->setText(QString::fromUtf8(obs_module_text(
      zoomed_ ? "StatusZoomActive" : "StatusZoomInactive")));
}

void CcuWindow::relayoutChannels() {
  if (!previewGrid_)
    return;
  // QGridLayout::addWidget doesn't remove a widget's previous cell when
  // re-adding it elsewhere, so re-placing frames on every zoom toggle
  // without first removing them piled up stale duplicate grid entries and
  // corrupted the layout after a couple of toggles.
  for (Channel &channel : channels_)
    previewGrid_->removeWidget(channel.frame);
  if (zoomed_) {
    for (int i = 0; i < 4; ++i)
      channels_[i].frame->setVisible(i == selected_);
    previewGrid_->addWidget(channels_[selected_].frame, 0, 0, 2, 2,
                            Qt::AlignTop);
  } else {
    for (int i = 0; i < 4; ++i) {
      previewGrid_->addWidget(channels_[i].frame, i / 2, i % 2, Qt::AlignTop);
      channels_[i].frame->setVisible(true);
    }
  }
  updatePreviewSizes();
}

void CcuWindow::updatePreviewSizes() {
  if (!leftPanel_ || !previewGrid_)
    return;
  // QGridLayout doesn't reliably honor heightForWidth, so the 16:9 preview
  // aspect ratio is driven explicitly from the actual column width instead
  // of letting each shell fight the layout reactively in its own
  // resizeEvent (that caused the preview to render at a stale, mismatched
  // size).
  const int totalWidth = leftPanel_->width();
  const int spacing = previewGrid_->horizontalSpacing();
  const int width =
      zoomed_ ? totalWidth : std::max(1, (totalWidth - spacing) / 2);
  const int height = previewShellHeightForWidth(width);
  for (Channel &channel : channels_)
    channel.previewShell->setFixedHeight(height);
}

void CcuWindow::resizeEvent(QResizeEvent *event) {
  QDialog::resizeEvent(event);
  updatePreviewSizes();
}

void CcuWindow::updateScopes() {
  if (!isVisible() || pickerActive_)
    return;
  obs_source_t *source = channels_[selected_].source;
  if (!source) {
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
  histogram_->setScopeData(scope);
  waveform_->setScopeData(scope);
  vectorscope_->setScopeData(scope);
}

void CcuWindow::sampleWhite(int channelIndex, const QPointF &normalized) {
  selectChannel(channelIndex);
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
  activeWindow->raise();
  activeWindow->activateWindow();
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
  });
}

void closeCcuWindow() {
  if (activeWindow)
    activeWindow->close();
  activeWindow = nullptr;
}
