#pragma once

#include <QDialog>

#include <array>

class QComboBox;
class QFrame;
class QGridLayout;
class QLabel;
class QPushButton;
class QResizeEvent;
class QSlider;
class QTimer;
class ObsDisplayWidget;
class ScopeWidget;
struct obs_source;
typedef struct obs_source obs_source_t;

class CcuWindow final : public QDialog {
public:
  explicit CcuWindow(QWidget *parent = nullptr);
  ~CcuWindow() override;

  void refreshSources();

protected:
  void resizeEvent(QResizeEvent *event) override;

private:
  struct Channel {
    QFrame *frame = nullptr;
    QFrame *previewShell = nullptr;
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
  void updateScopes();
  void relayoutChannels();
  void updatePreviewSizes();
  obs_source_t *ensureFilter(obs_source_t *source);
  void loadAssignments();
  void saveAssignments() const;

  std::array<Channel, 4> channels_{};
  std::array<QPushButton *, 4> selectButtons_{};
  int selected_ = 0;
  bool pickerActive_ = false;
  bool zoomed_ = false;
  QWidget *leftPanel_ = nullptr;
  QGridLayout *previewGrid_ = nullptr;
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
  ScopeWidget *histogram_ = nullptr;
  ScopeWidget *waveform_ = nullptr;
  ScopeWidget *vectorscope_ = nullptr;
  QTimer *scopeTimer_ = nullptr;
  bool updating_ = false;
};

void showCcuWindow();
void closeCcuWindow();
