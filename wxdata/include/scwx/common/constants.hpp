#pragma once

#include <cstdint>

namespace scwx::common
{

constexpr std::uint32_t MAX_1_DEGREE_RADIALS    = 360;
constexpr std::uint32_t MAX_0_5_DEGREE_RADIALS  = 720;
constexpr std::uint32_t MAX_0_25_DEGREE_RADIALS = 1440;
constexpr std::uint32_t MAX_AZIMUTH_NUMBER      = 1501;
constexpr std::uint32_t MAX_DATA_MOMENT_GATES   = 4800;

// One extra radial slot is reserved so incomplete scans can insert a closing
// vertex radial without stretching the last bin across the gap.
constexpr std::uint32_t MAX_RADIALS = MAX_AZIMUTH_NUMBER + 1u;

// Coordinates store one extra range vertex per radial (outer edge of the last
// gate). Indexing with MAX_DATA_MOMENT_GATES aliases that vertex onto the next
// radial's first gate and draws huge inner-gate triangles over the sweep.
constexpr std::uint32_t MAX_DATA_MOMENT_GATE_COORDINATES =
   MAX_DATA_MOMENT_GATES + 1u;

} // namespace scwx::common
