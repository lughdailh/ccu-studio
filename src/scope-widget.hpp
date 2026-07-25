#pragma once

#include "scope-data.hpp"

#include <QWidget>

class ScopeWidget final : public QWidget {
public:
  enum class Type { Histogram, Waveform, Vectorscope };

  explicit ScopeWidget(Type type, QWidget *parent = nullptr);
  void setScopeData(const ScopeData &data);
  void clear();

protected:
  void paintEvent(QPaintEvent *) override;

private:
  void paintHistogram(QPainter &painter, const QRectF &area);
  void paintWaveform(QPainter &painter, const QRectF &area);
  void paintVectorscope(QPainter &painter, const QRectF &area);
  void paintGrid(QPainter &painter, const QRectF &area, int divisions);

  Type type_;
  ScopeData data_;
};

