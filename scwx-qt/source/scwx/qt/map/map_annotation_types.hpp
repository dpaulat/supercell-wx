#pragma once

#include <scwx/common/geographic.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include <units/length.h>

namespace scwx::qt::map
{

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
enum class MapAnnotationTool : std::uint8_t
{
   None,
   Freehand,
   Line,
   Circle,
   Rectangle,
   Measure,
   Erase
};

enum class MapAnnotationLineJoin : std::uint8_t
{
   Miter,
   Bevel,
   Round
};

enum class MapAnnotationStrokeStyle : std::uint8_t
{
   Solid,
   Dashed,
   Hatched
};

struct MapAnnotationStyle
{
   units::length::meters<double> strokeWidthM {500.0};
   std::array<float, 4>          strokeColor {1.f, 0.2f, 0.2f, 0.9f};
   std::array<float, 4>          fillColor {1.f, 0.2f, 0.2f, 0.25f};
   MapAnnotationLineJoin         lineJoin {MapAnnotationLineJoin::Miter};
   MapAnnotationStrokeStyle      strokeStyle {MapAnnotationStrokeStyle::Solid};
   bool                          polygonFill {false};
   bool                          hatchFill {false};
   units::length::meters<double> dashPeriodM {4000.0};
};

struct MapAnnotationPolyline
{
   std::vector<common::Coordinate> points;
   /** When true (freehand), stroke is built from overlapping geo disks (round
    * brush). */
   bool roundStroke {false};
};

struct MapAnnotationCircle
{
   common::Coordinate center;
   double             radiusMeters {};
};

struct MapAnnotationRectangle
{
   common::Coordinate corner1;
   common::Coordinate corner2;
   bool               fill {false};
   bool               hatchFill {false};
};

struct MapAnnotationMeasure
{
   common::Coordinate a;
   common::Coordinate b;
};

using MapAnnotationPayload = std::variant<MapAnnotationPolyline,
                                          MapAnnotationCircle,
                                          MapAnnotationRectangle,
                                          MapAnnotationMeasure>;

struct MapAnnotationObject
{
   std::uint64_t        id {};
   MapAnnotationPayload payload;
   MapAnnotationStyle   style;
};

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers)

} // namespace scwx::qt::map
