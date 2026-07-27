#include "scope-widget.hpp"

#include <obs-module.h>

#include <QImage>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

namespace {
double intensity(std::uint32_t count, std::uint32_t maximum) {
  if (!count || !maximum)
    return 0.0;
  return std::log1p(static_cast<double>(count)) /
         std::log1p(static_cast<double>(maximum));
}
} // namespace

ScopeWidget::ScopeWidget(Type type, QWidget *parent)
    : QWidget(parent), type_(type) {
  setMinimumSize(260, 190);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void ScopeWidget::setScopeData(const ScopeData &data) {
  data_ = data;
  update();
}

void ScopeWidget::setReferenceData(const ScopeData &data) {
  referenceData_ = data;
  update();
}

void ScopeWidget::clearReference() {
  referenceData_ = ScopeData{};
  update();
}

void ScopeWidget::clear() {
  data_ = ScopeData{};
  update();
}

void ScopeWidget::paintEvent(QPaintEvent *) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.fillRect(rect(), QColor(13, 15, 18));
  const QRectF area = QRectF(rect()).adjusted(12, 10, -12, -20);
  if (data_.empty() && referenceData_.empty()) {
    painter.setPen(QColor(155, 160, 168));
    painter.drawText(area, Qt::AlignCenter,
                     QString::fromUtf8(obs_module_text("ScopeNoSource")));
    return;
  }
  switch (type_) {
  case Type::Histogram:
    if (!data_.empty())
      paintHistogram(painter, area, data_, false);
    if (!referenceData_.empty())
      paintHistogram(painter, area, referenceData_, !data_.empty());
    break;
  case Type::Waveform:
    if (!data_.empty())
      paintWaveform(painter, area, data_, false);
    if (!referenceData_.empty())
      paintWaveform(painter, area, referenceData_, !data_.empty());
    break;
  case Type::Vectorscope:
    if (!data_.empty())
      paintVectorscope(painter, area, data_, false);
    if (!referenceData_.empty())
      paintVectorscope(painter, area, referenceData_, !data_.empty());
    break;
  }
  if (!referenceData_.empty()) {
    const QRectF badge(area.right() - 42, area.top() + 5, 38, 18);
    painter.fillRect(badge, QColor(255, 176, 45, 195));
    painter.setPen(QColor(20, 20, 20));
    painter.drawText(badge, Qt::AlignCenter, QStringLiteral("REF"));
  }
}

void ScopeWidget::paintGrid(QPainter &painter, const QRectF &area,
                            int divisions) {
  painter.save();
  painter.setPen(QPen(QColor(115, 122, 132, 55), 1));
  for (int i = 0; i <= divisions; ++i) {
    const double fraction = static_cast<double>(i) / divisions;
    painter.drawLine(QPointF(area.left() + area.width() * fraction, area.top()),
                     QPointF(area.left() + area.width() * fraction,
                             area.bottom()));
    painter.drawLine(QPointF(area.left(), area.top() + area.height() * fraction),
                     QPointF(area.right(),
                             area.top() + area.height() * fraction));
  }
  painter.restore();
}

void ScopeWidget::paintHistogram(QPainter &painter, const QRectF &area,
                                 const ScopeData &data, bool reference) {
  if (!reference)
    paintGrid(painter, area, 4);
  const QColor colors[] = {QColor(255, 78, 78, 220),
                           QColor(75, 235, 118, 220),
                           QColor(80, 145, 255, 230)};
  for (int channel = 0; channel < 3; ++channel) {
    const auto &bins = data.histogram[channel];
    const std::uint32_t maximum =
        *std::max_element(bins.begin(), bins.end());
    QPainterPath path;
    for (int i = 0; i < ScopeData::HistogramBins; ++i) {
      const double x =
          area.left() + area.width() * i / (ScopeData::HistogramBins - 1);
      const double y =
          area.bottom() - area.height() * intensity(bins[i], maximum);
      if (i == 0)
        path.moveTo(x, y);
      else
        path.lineTo(x, y);
    }
    QPen pen(reference ? QColor(255, 184, 65, 175) : colors[channel],
             reference ? 1.4 : 1.6);
    if (reference)
      pen.setStyle(Qt::DashLine);
    painter.setPen(pen);
    painter.drawPath(path);
  }
  if (!reference) {
    painter.setPen(QColor(180, 185, 192));
    painter.drawText(QPointF(area.left(), height() - 4), QStringLiteral("0"));
    painter.drawText(QPointF(area.right() - 22, height() - 4),
                     QStringLiteral("255"));
  }
}

void ScopeWidget::paintWaveform(QPainter &painter, const QRectF &area,
                                const ScopeData &data, bool reference) {
  QImage image(ScopeData::WaveformWidth, ScopeData::WaveformHeight,
               QImage::Format_ARGB32);
  image.fill(Qt::transparent);
  const std::uint32_t maximum =
      *std::max_element(data.waveform.begin(), data.waveform.end());
  for (int y = 0; y < ScopeData::WaveformHeight; ++y) {
    auto *line = reinterpret_cast<QRgb *>(image.scanLine(y));
    for (int x = 0; x < ScopeData::WaveformWidth; ++x) {
      const std::uint32_t count =
          data.waveform[y * ScopeData::WaveformWidth + x];
      const int alpha =
          static_cast<int>(std::lround(intensity(count, maximum) * 235));
      line[x] = reference ? qRgba(255, 178, 55, alpha * 3 / 4)
                          : qRgba(110, 255, 155, alpha);
    }
  }
  painter.drawImage(area, image);
  if (!reference) {
    paintGrid(painter, area, 4);
    painter.setPen(QColor(180, 185, 192));
    painter.drawText(QPointF(area.left() + 4, area.top() + 13),
                     QStringLiteral("100"));
    painter.drawText(QPointF(area.left() + 4, area.bottom() - 4),
                     QStringLiteral("0"));
  }
}

void ScopeWidget::paintVectorscope(QPainter &painter, const QRectF &area,
                                   const ScopeData &data, bool reference) {
  const double side = std::min(area.width(), area.height());
  const QRectF square(area.center().x() - side / 2.0,
                      area.center().y() - side / 2.0, side, side);
  if (!reference) {
    painter.setPen(QPen(QColor(120, 130, 140, 90), 1));
    painter.drawEllipse(square.adjusted(2, 2, -2, -2));
    painter.drawLine(QPointF(square.center().x(), square.top()),
                     QPointF(square.center().x(), square.bottom()));
    painter.drawLine(QPointF(square.left(), square.center().y()),
                     QPointF(square.right(), square.center().y()));
    painter.drawEllipse(
        QRectF(square.center() - QPointF(side * 0.25, side * 0.25),
               QSizeF(side * 0.5, side * 0.5)));
  }

  QImage image(ScopeData::VectorscopeSize, ScopeData::VectorscopeSize,
               QImage::Format_ARGB32);
  image.fill(Qt::transparent);
  const std::uint32_t maximum = *std::max_element(
      data.vectorscope.begin(), data.vectorscope.end());
  for (int y = 0; y < ScopeData::VectorscopeSize; ++y) {
    auto *line = reinterpret_cast<QRgb *>(image.scanLine(y));
    for (int x = 0; x < ScopeData::VectorscopeSize; ++x) {
      const std::uint32_t count =
          data.vectorscope[y * ScopeData::VectorscopeSize + x];
      const int alpha =
          static_cast<int>(std::lround(intensity(count, maximum) * 245));
      line[x] = reference ? qRgba(255, 178, 55, alpha * 3 / 4)
                          : qRgba(165, 255, 175, alpha);
    }
  }
  painter.drawImage(square, image);

  if (!reference) {
    painter.setPen(QColor(190, 195, 202));
    painter.drawText(QPointF(square.center().x() - 4, square.top() + 13),
                     QStringLiteral("R"));
    painter.drawText(QPointF(square.right() - 18, square.center().y() - 4),
                     QStringLiteral("B"));
    painter.drawText(QPointF(square.center().x() - 4, square.bottom() - 4),
                     QStringLiteral("Cy"));
    painter.drawText(QPointF(square.left() + 5, square.center().y() - 4),
                     QStringLiteral("Yl"));
  }
}
