#include "scope-data.hpp"

#include <algorithm>
#include <cmath>

ScopeData::ScopeData()
    : waveform(WaveformWidth * WaveformHeight),
      vectorscope(VectorscopeSize * VectorscopeSize) {}

ScopeData analyzeScopeFrame(const std::uint8_t *rgba, std::uint32_t width,
                            std::uint32_t height, std::uint32_t stride) {
  ScopeData result;
  if (!rgba || !width || !height || stride < width * 4)
    return result;

  const std::uint32_t stepX = std::max(1u, width / 480u);
  const std::uint32_t stepY = std::max(1u, height / 270u);
  for (std::uint32_t y = 0; y < height; y += stepY) {
    const std::uint8_t *row = rgba + y * stride;
    for (std::uint32_t x = 0; x < width; x += stepX) {
      const std::uint8_t *pixel = row + x * 4;
      const int red = pixel[0];
      const int green = pixel[1];
      const int blue = pixel[2];
      ++result.histogram[0][red];
      ++result.histogram[1][green];
      ++result.histogram[2][blue];

      const double luminance =
          0.2126 * red + 0.7152 * green + 0.0722 * blue;
      const int waveX = width > 1
                            ? static_cast<int>(
                                  x * (ScopeData::WaveformWidth - 1) /
                                  (width - 1))
                            : 0;
      const int waveY = std::clamp(
          ScopeData::WaveformHeight - 1 -
              static_cast<int>(std::lround(
                  luminance * (ScopeData::WaveformHeight - 1) / 255.0)),
          0, ScopeData::WaveformHeight - 1);
      ++result.waveform[waveY * ScopeData::WaveformWidth + waveX];

      const double normalizedY = luminance / 255.0;
      const double cb = 0.5 + (blue / 255.0 - normalizedY) * 0.564;
      const double cr = 0.5 + (red / 255.0 - normalizedY) * 0.713;
      const int vectorX = std::clamp(
          static_cast<int>(
              std::lround(cb * (ScopeData::VectorscopeSize - 1))),
          0, ScopeData::VectorscopeSize - 1);
      const int vectorY = std::clamp(
          static_cast<int>(
              std::lround((1.0 - cr) * (ScopeData::VectorscopeSize - 1))),
          0, ScopeData::VectorscopeSize - 1);
      ++result
            .vectorscope[vectorY * ScopeData::VectorscopeSize + vectorX];
      ++result.samples;
    }
  }
  return result;
}

