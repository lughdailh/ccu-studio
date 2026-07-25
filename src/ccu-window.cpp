#include "ccu-window.hpp"
#include "obs-display-widget.hpp"

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
#include <QScreen>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>

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

struct SliderSpec {
  const char *label;
  int minimum;
  int maximum;
  int neutral;
};
constexpr SliderSpec specs[] = {
    {"Vermell", 50, 200, 100},   {"Verd", 50, 200, 100},
    {"Blau", 50, 200, 100},      {"Brillantor", -100, 100, 0},
    {"Contrast", 0, 200, 100},   {"Gamma", 20, 300, 100},
    {"Saturació", 0, 200, 100},
};

bool enumerateVideoSources(void *data, obs_source_t *source) {
  auto *names = static_cast<QStringList *>(data);
  if ((obs_source_get_output_flags(source) & OBS_SOURCE_VIDEO) &&
      obs_source_get_type(source) == OBS_SOURCE_TYPE_INPUT)
    names->push_back(QString::fromUtf8(obs_source_get_name(source)));
  return true;
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
  if (!source)
    return false;
  const uint32_t sourceWidth = obs_source_get_width(source);
  const uint32_t sourceHeight = obs_source_get_height(source);
  if (!sourceWidth || !sourceHeight)
    return false;

  const uint32_t width = std::min(sourceWidth, 960u);
  const uint32_t height =
      std::max(1u, static_cast<uint32_t>(
                       std::lround(width * (double(sourceHeight) / sourceWidth))));
  gs_texrender_t *render = nullptr;
  gs_stagesurf_t *stage = nullptr;
  bool success = false;
  obs_enter_graphics();
  render = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
  stage = gs_stagesurface_create(width, height, GS_RGBA);
  if (render && stage &&
      gs_texrender_begin_with_color_space(render, width, height, GS_CS_SRGB)) {
    vec4 clear{};
    gs_clear(GS_CLEAR_COLOR, &clear, 0.0f, 0);
    gs_ortho(0.0f, static_cast<float>(sourceWidth), 0.0f,
             static_cast<float>(sourceHeight), -100.0f, 100.0f);
    gs_set_viewport(0, 0, width, height);
    obs_source_video_render(source);
    gs_texrender_end(render);
    gs_stage_texture(stage, gs_texrender_get_texture(render));

    uint8_t *pixels = nullptr;
    uint32_t stride = 0;
    if (gs_stagesurface_map(stage, &pixels, &stride)) {
      const int centerX = std::clamp(
          static_cast<int>(normalized.x() * (width - 1)), 0, int(width - 1));
      const int centerY = std::clamp(
          static_cast<int>(normalized.y() * (height - 1)), 0, int(height - 1));
      double sums[3]{};
      int count = 0;
      for (int y = std::max(0, centerY - 3);
           y <= std::min(int(height - 1), centerY + 3); ++y) {
        for (int x = std::max(0, centerX - 3);
             x <= std::min(int(width - 1), centerX + 3); ++x) {
          const uint8_t *pixel = pixels + y * stride + x * 4;
          sums[0] += pixel[0];
          sums[1] += pixel[1];
          sums[2] += pixel[2];
          ++count;
        }
      }
      if (count) {
        for (int i = 0; i < 3; ++i)
          rgb[i] = sums[i] / (255.0 * count);
        success = true;
      }
      gs_stagesurface_unmap(stage);
    }
  }
  gs_stagesurface_destroy(stage);
  gs_texrender_destroy(render);
  obs_leave_graphics();
  return success;
}
} // namespace

CcuWindow::CcuWindow(QWidget *parent) : QDialog(parent) {
  setWindowTitle(QStringLiteral("CCU OBS — prova ") +
                 QString::fromLatin1(CCU_VERSION));
  setWindowFlag(Qt::Window, true);
  setModal(false);
  auto *mainLayout = new QVBoxLayout(this);
  auto *previews = new QHBoxLayout;
  previews->setSpacing(8);
  for (int i = 0; i < 4; ++i) {
    Channel &channel = channels_[i];
    channel.frame = new QFrame(this);
    channel.frame->setObjectName(QStringLiteral("channel%1").arg(i + 1));
    auto *layout = new QVBoxLayout(channel.frame);
    layout->setContentsMargins(3, 3, 3, 3);
    channel.sourceBox = new QComboBox(channel.frame);
    channel.preview = new ObsDisplayWidget(channel.frame);
    auto *label = new QLabel(QStringLiteral("Càmera %1").arg(i + 1), channel.frame);
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(channel.sourceBox);
    layout->addWidget(channel.preview, 1);
    layout->addWidget(label);
    previews->addWidget(channel.frame, 1);
    connect(channel.sourceBox, &QComboBox::currentIndexChanged, this,
            [this, i](int) { chooseSource(i); });
    channel.preview->setClickHandler([this, i](const QPointF &position) {
      if (pickerActive_)
        sampleWhite(i, position);
      else
        selectChannel(i);
    });
  }
  mainLayout->addLayout(previews, 1);

  auto *selectors = new QHBoxLayout;
  selectors->addStretch();
  selectors->addWidget(new QLabel(QStringLiteral("Càmera activa:"), this));
  for (int i = 0; i < 4; ++i) {
    selectButtons_[i] = new QPushButton(QString::number(i + 1), this);
    selectButtons_[i]->setCheckable(true);
    selectButtons_[i]->setMinimumWidth(52);
    selectors->addWidget(selectButtons_[i]);
    connect(selectButtons_[i], &QPushButton::clicked, this,
            [this, i] { selectChannel(i); });
  }
  selectors->addStretch();
  mainLayout->addLayout(selectors);

  auto *tools = new QHBoxLayout;
  pickerButton_ = new QPushButton(QStringLiteral("⌖  Comptagotes"), this);
  pickerButton_->setCheckable(true);
  zoomButton_ = new QPushButton(QStringLiteral("🔍  Lupa"), this);
  zoomButton_->setCheckable(true);
  auto *reset = new QPushButton(QStringLiteral("Restablir"), this);
  status_ = new QLabel(QStringLiteral("Selecciona una font per començar."), this);
  tools->addWidget(pickerButton_);
  tools->addWidget(zoomButton_);
  tools->addWidget(reset);
  tools->addWidget(status_, 1);
  mainLayout->addLayout(tools);
  connect(pickerButton_, &QPushButton::clicked, this, [this] { togglePicker(); });
  connect(zoomButton_, &QPushButton::clicked, this, [this] { toggleZoom(); });
  connect(reset, &QPushButton::clicked, this, [this] { resetControls(); });

  auto *controls = new QGridLayout;
  QSlider **sliders[] = {&red_, &green_, &blue_, &brightness_,
                         &contrast_, &gamma_, &saturation_};
  for (int i = 0; i < 7; ++i) {
    auto *label = new QLabel(QString::fromUtf8(specs[i].label), this);
    *sliders[i] = new QSlider(Qt::Horizontal, this);
    (*sliders[i])->setRange(specs[i].minimum, specs[i].maximum);
    (*sliders[i])->setValue(specs[i].neutral);
    auto *value = new QLabel(this);
    value->setMinimumWidth(48);
    controls->addWidget(label, i / 2, (i % 2) * 3);
    controls->addWidget(*sliders[i], i / 2, (i % 2) * 3 + 1);
    controls->addWidget(value, i / 2, (i % 2) * 3 + 2);
    connect(*sliders[i], &QSlider::valueChanged, this,
            [this, value, i](int number) {
              value->setText(QString::number(settingValue(i, number), 'f', 2));
              applyControls();
            });
    value->setText(QString::number(settingValue(i, specs[i].neutral), 'f', 2));
  }
  mainLayout->addLayout(controls);
  refreshSources();
  loadAssignments();
  selectChannel(0);

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
    channel.sourceBox->addItem(QStringLiteral("— Sense font —"));
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
    channels_[i].frame->setStyleSheet(
        active ? QStringLiteral("QFrame#channel%1 { border: 3px solid #e6b422; }")
                     .arg(i + 1)
               : QString());
  }
  if (zoomed_)
    for (int i = 0; i < 4; ++i)
      channels_[i].frame->setVisible(i == selected_);
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
        QStringLiteral("Editant la càmera %1: %2")
            .arg(selected_ + 1)
            .arg(channels_[selected_].sourceBox->currentText()));
  } else {
    for (int i = 0; i < 7; ++i)
      sliders[i]->setValue(specs[i].neutral);
    status_->setText(QStringLiteral("La càmera %1 no té cap font.").arg(selected_ + 1));
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
  status_->setText(QStringLiteral("Correcció restablerta."));
}

void CcuWindow::togglePicker() {
  pickerActive_ = pickerButton_->isChecked();
  for (Channel &channel : channels_)
    channel.preview->setPickMode(pickerActive_);
  status_->setText(
      pickerActive_
          ? QStringLiteral("Comptagotes actiu: clica només dins del vídeo.")
          : QStringLiteral("Comptagotes desactivat."));
}

void CcuWindow::toggleZoom() {
  zoomed_ = zoomButton_->isChecked();
  for (int i = 0; i < 4; ++i)
    channels_[i].frame->setVisible(!zoomed_ || i == selected_);
  status_->setText(zoomed_ ? QStringLiteral("Vista ampliada de la càmera activa.")
                           : QStringLiteral("Vista de quatre càmeres."));
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
          QStringLiteral("Mostra RGB: %1, %2, %3")
              .arg(sample[0], 0, 'f', 3)
              .arg(sample[1], 0, 'f', 3)
              .arg(sample[2], 0, 'f', 3));
    }
  } else {
    status_->setText(QStringLiteral("No s'ha pogut llegir aquest fotograma."));
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
}

void closeCcuWindow() {
  if (activeWindow)
    activeWindow->close();
  activeWindow = nullptr;
}
