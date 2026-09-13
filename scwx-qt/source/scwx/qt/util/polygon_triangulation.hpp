#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace scwx::qt::util
{

using PolygonRing2D = std::vector<std::array<double, 2>>;

template<typename Index = std::uint32_t>
[[nodiscard]] std::vector<Index>
TriangulatePolygon(const std::vector<PolygonRing2D>& rings);

} // namespace scwx::qt::util
