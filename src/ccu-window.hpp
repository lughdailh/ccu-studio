#pragma once

#include "scope-data.hpp"

#include <QDialog>

#include <array>

class QComboBox;
class QFrame;
class QLabel;
class QPushButton;
class QToolButton;
class QResizeEvent;
class QSlider;
class QTimer;
class QTabWidget;
class QVBoxLayout;
class ObsDisplayWidget;
class ScopeWidget;
struct obs_source;
typedef struct obs_source obs_source_t;

class CcuWindow final : public QDialog {
public:
  explicit CcuWindow(QWidget *parent = nullptr);
  ~CcuWindow() override;

  void refreshSources();
  void applyWindowAspectRatio();

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
  void showSourceMenu(int index, const QPoint &globalPosition);
  void updateSelectedUi();
  void applyControls();
  void resetControls();
  void togglePicker();
  void toggleZoom();
  void toggleScopeFreeze();
  void toggleCompare();
  void updateComparePreview();
  void sampleWhite(int channel, const QPointF &normalized);
  void updateScopes();
  void relayoutChannels();
  void updatePreviewSizes();
  obs_source_t *ensureFilter(obs_source_t *source);
  void loadAssignments();
  void saveAssignments() const;

  std::array<Channel, 4> channels_{};
  std::array<QToolButton *, 4> selectButtons_{};
  int selected_ = 0;
  bool pickerActive_ = false;
  bool zoomed_ = false;
  QWidget *leftPanel_ = nullptr;
  QWidget *rightPanel_ = nullptr;
  QWidget *previewContainer_ = nullptr;
  QVBoxLayout *leftLayout_ = nullptr;
  QToolButton *pickerButton_ = nullptr;
  QToolButton *zoomButton_ = nullptr;
  QToolButton *freezeButton_ = nullptr;
  QToolButton *compareButton_ = nullptr;
  QToolButton *instructionsButton_ = nullptr;
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
  QTabWidget *scopeTabs_ = nullptr;
  QTimer *scopeTimer_ = nullptr;
  ScopeData lastScopeData_;
  int lastScopeChannel_ = -1;
  int frozenScopeChannel_ = -1;
  bool updating_ = false;
  bool previewResizePending_ = false;
};

void showCcuWindow();
void closeCcuWindow();
