#pragma once

#include <array>
#include <cstdint>
#include <vector>

struct ScopeData {
  static constexpr int HistogramBins = 256;
  static constexpr int WaveformWidth = 256;
  static constexpr int WaveformHeight = 128;
  static constexpr int VectorscopeSize = 192;

  std::array<std::array<std::uint32_t, HistogramBins>, 3> histogram{};
  std::vector<std::uint32_t> waveform;
  std::vector<std::uint32_t> vectorscope;
  std::uint32_t samples = 0;

  ScopeData();
  bool empty() const { return samples == 0; }
};

ScopeData analyzeScopeFrame(const std::uint8_t *rgba, std::uint32_t width,
                            std::uint32_t height, std::uint32_t stride);

