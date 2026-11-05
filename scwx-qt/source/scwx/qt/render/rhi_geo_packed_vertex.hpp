#pragma once

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <vector>

namespace scwx::qt::render
{

/**
 * Packed vertex for static (lat/long based) geo-colored geometry, 28 bytes
 * rather than the 48 bytes of 9 floats + 3 integers. Drawn with the same
 * shader as the compact layout; the narrower formats are expanded by the
 * vertex fetch stage.
 */
struct GeoPackedVertex
{
   float         latitude_;
   float         longitude_;
   std::uint16_t xOffsetHalf_;   // Half float, pixels
   std::uint16_t yOffsetHalf_;   // Half float, pixels
   std::uint8_t  rgba_[4];       // Normalized color
   std::uint16_t perpendicular_; // Half float
   std::int16_t  threshold_;     // Nautical miles
   std::int32_t  startTime_;     // Minutes since epoch
   std::int32_t  endTime_;       // Minutes since epoch
};

static_assert(sizeof(GeoPackedVertex) == 28);

/// Converts a float to IEEE 754 binary16 bits (round to nearest even).
[[nodiscard]] inline std::uint16_t FloatToHalfBits(float value)
{
   const std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
   const std::uint32_t sign = (bits >> 16) & 0x8000u;
   const std::uint32_t mag  = bits & 0x7FFFFFFFu;

   if (mag >= 0x7F800000u)
   {
      // Infinity or NaN
      return static_cast<std::uint16_t>(sign | 0x7C00u |
                                        (mag > 0x7F800000u ? 0x0200u : 0u));
   }
   if (mag >= 0x477FF000u)
   {
      // Rounds to or overflows past the largest half: infinity
      return static_cast<std::uint16_t>(sign | 0x7C00u);
   }
   if (mag < 0x38800000u)
   {
      // Subnormal half or zero
      if (mag < 0x33000000u)
      {
         return static_cast<std::uint16_t>(sign);
      }
      const std::uint32_t exponent = mag >> 23;
      const std::uint32_t mantissa = (mag & 0x007FFFFFu) | 0x00800000u;
      const std::uint32_t shift    = 126u - exponent;
      std::uint32_t       half     = mantissa >> shift;
      const std::uint32_t rem      = mantissa & ((1u << shift) - 1u);
      const std::uint32_t halfway  = 1u << (shift - 1u);
      if (rem > halfway || (rem == halfway && (half & 1u) != 0u))
      {
         ++half;
      }
      return static_cast<std::uint16_t>(sign | half);
   }

   // Normal half: rebias the exponent and round the mantissa
   std::uint32_t       half = ((mag - 0x38000000u) >> 13);
   const std::uint32_t rem  = mag & 0x1FFFu;
   if (rem > 0x1000u || (rem == 0x1000u && (half & 1u) != 0u))
   {
      ++half;
   }
   return static_cast<std::uint16_t>(sign | half);
}

[[nodiscard]] inline std::uint8_t NormalizedByte(float value)
{
   return static_cast<std::uint8_t>(
      std::clamp(std::lround(value * 255.0f), 0L, 255L));
}

[[nodiscard]] inline GeoPackedVertex MakeGeoPackedVertex(float        latitude,
                                                         float        longitude,
                                                         float        xOffset,
                                                         float        yOffset,
                                                         std::uint8_t r,
                                                         std::uint8_t g,
                                                         std::uint8_t b,
                                                         std::uint8_t a,
                                                         float        perp,
                                                         std::int32_t threshold,
                                                         std::int32_t startTime,
                                                         std::int32_t endTime)
{
   return GeoPackedVertex {latitude,
                           longitude,
                           FloatToHalfBits(xOffset),
                           FloatToHalfBits(yOffset),
                           {r, g, b, a},
                           FloatToHalfBits(perp),
                           static_cast<std::int16_t>(std::clamp<std::int32_t>(
                              threshold, -32768, 32767)),
                           startTime,
                           endTime};
}

/// Immutable, shareable vertex buffer. Every pane has its own placefile
/// layer, but the geometry is the same for all of them.
using GeoSharedBuffer = std::shared_ptr<const std::vector<GeoPackedVertex>>;

/**
 * Takes ownership of freshly built geometry and returns a shared buffer. If an
 * identical buffer is already alive (e.g. built by another pane for the same
 * placefile), that one is returned and the new vertices are freed, so N panes
 * hold one CPU copy rather than N. Equality is by content, so it can never
 * return a stale buffer.
 */
[[nodiscard]] inline GeoSharedBuffer
InternGeoBuffer(std::vector<GeoPackedVertex>&& vertices)
{
   static std::mutex                                                     mutex;
   static std::vector<std::weak_ptr<const std::vector<GeoPackedVertex>>> live;

   if (vertices.capacity() - vertices.size() > vertices.size() / 16)
   {
      vertices.shrink_to_fit();
   }

   std::scoped_lock lock {mutex};

   std::erase_if(live, [](const auto& w) { return w.expired(); });

   for (const auto& w : live)
   {
      if (auto existing = w.lock();
          existing != nullptr && existing->size() == vertices.size() &&
          std::memcmp(existing->data(),
                      vertices.data(),
                      vertices.size() * sizeof(GeoPackedVertex)) == 0)
      {
         return existing;
      }
   }

   auto buffer =
      std::make_shared<const std::vector<GeoPackedVertex>>(std::move(vertices));
   live.push_back(buffer);
   return buffer;
}

} // namespace scwx::qt::render
