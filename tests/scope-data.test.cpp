#include "scope-data.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>

int main() {
  constexpr std::uint32_t width = 4;
  constexpr std::uint32_t height = 2;
  const std::array<std::uint8_t, width * height * 4> pixels = {
      255, 0,   0,   255, 0,   255, 0,   255, 0,   0,   255,
      255, 255, 255, 255, 255, 0,   0,   0,   255, 128, 128, 128,
      255, 255, 255, 255, 255, 64,  64,  64,  255};

  const ScopeData data =
      analyzeScopeFrame(pixels.data(), width, height, width * 4);
  assert(data.samples == width * height);
  assert(data.histogram[0][255] == 3);
  assert(data.histogram[1][255] == 3);
  assert(data.histogram[2][255] == 3);
  assert(data.histogram[0][0] == 3);

  const auto waveformMaximum =
      *std::max_element(data.waveform.begin(), data.waveform.end());
  const auto vectorscopeMaximum =
      *std::max_element(data.vectorscope.begin(), data.vectorscope.end());
  assert(waveformMaximum > 0);
  assert(vectorscopeMaximum > 0);

  const ScopeData empty = analyzeScopeFrame(nullptr, 0, 0, 0);
  assert(empty.empty());
  return 0;
}
