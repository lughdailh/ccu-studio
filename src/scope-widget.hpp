#pragma once

#include "scope-data.hpp"

#include <QWidget>

class ScopeWidget final : public QWidget {
public:
  enum class Type { Histogram, Waveform, Vectorscope };

  explicit ScopeWidget(Type type, QWidget *parent = nullptr);
  void setScopeData(const ScopeData &data);
  void setReferenceData(const ScopeData &data);
  void clearReference();
  void clear();

protected:
  void paintEvent(QPaintEvent *) override;

private:
  void paintHistogram(QPainter &painter, const QRectF &area,
                      const ScopeData &data, bool reference);
  void paintWaveform(QPainter &painter, const QRectF &area,
                     const ScopeData &data, bool reference);
  void paintVectorscope(QPainter &painter, const QRectF &area,
                        const ScopeData &data, bool reference);
  void paintGrid(QPainter &painter, const QRectF &area, int divisions);

  Type type_;
  ScopeData data_;
  ScopeData referenceData_;
};
