#pragma once

#include <QDialog>

#include <array>

class QComboBox;
class QFrame;
class QLabel;
class QPushButton;
class QSlider;
class ObsDisplayWidget;
struct obs_source;
typedef struct obs_source obs_source_t;

class CcuWindow final : public QDialog {
public:
  explicit CcuWindow(QWidget *parent = nullptr);
  ~CcuWindow() override;

  void refreshSources();

private:
  struct Channel {
    QFrame *frame = nullptr;
    QComboBox *sourceBox = nullptr;
    ObsDisplayWidget *preview = nullptr;
    obs_source_t *source = nullptr;
  };

  void selectChannel(int index);
  void chooseSource(int index);
  void updateSelectedUi();
  void applyControls();
  void resetControls();
  void togglePicker();
  void toggleZoom();
  void sampleWhite(int channel, const QPointF &normalized);
  obs_source_t *ensureFilter(obs_source_t *source);
  void loadAssignments();
  void saveAssignments() const;

  std::array<Channel, 4> channels_{};
  std::array<QPushButton *, 4> selectButtons_{};
  int selected_ = 0;
  bool pickerActive_ = false;
  bool zoomed_ = false;
  QPushButton *pickerButton_ = nullptr;
  QPushButton *zoomButton_ = nullptr;
  QLabel *status_ = nullptr;
  QSlider *red_ = nullptr;
  QSlider *green_ = nullptr;
  QSlider *blue_ = nullptr;
  QSlider *brightness_ = nullptr;
  QSlider *contrast_ = nullptr;
  QSlider *gamma_ = nullptr;
  QSlider *saturation_ = nullptr;
  bool updating_ = false;
};

void showCcuWindow();
void closeCcuWindow();
