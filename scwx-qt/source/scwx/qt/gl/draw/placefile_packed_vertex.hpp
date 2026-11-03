#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <vector>

namespace scwx::qt::gl::draw
{

/**
 * Packed vertex for placefile lines, 32 bytes rather than the 48 bytes of 9
 * floats + 3 integers. The narrower formats are expanded by the vertex fetch
 * (GL_HALF_FLOAT, normalized GL_UNSIGNED_BYTE, GL_SHORT), so the shaders are
 * unchanged.
 */
struct PackedLineVertex
{
   float                       latitude_;
   float                       longitude_;
   std::uint16_t               xOffsetHalf_; // Half float, pixels
   std::uint16_t               yOffsetHalf_; // Half float, pixels
   std::array<std::uint8_t, 4> rgba_;        // Normalized color
   float                       angle_;       // Degrees
   std::int16_t                threshold_;   // Nautical miles
   std::int16_t                padding_;
   std::int32_t                startTime_; // Minutes since epoch
   std::int32_t                endTime_;   // Minutes since epoch
};

static constexpr std::size_t kPackedLineVertexSize = 32;
static_assert(sizeof(PackedLineVertex) == kPackedLineVertexSize);

/**
 * Packed vertex for placefile triangles and polygons, 28 bytes rather than the
 * 44 bytes of 8 floats + 3 integers.
 */
struct PackedFillVertex
{
   float                       screenX_;
   float                       screenY_;
   std::uint16_t               xOffsetHalf_; // Half float, pixels
   std::uint16_t               yOffsetHalf_; // Half float, pixels
   std::array<std::uint8_t, 4> rgba_;        // Normalized color
   std::int16_t                threshold_;   // Nautical miles
   std::int16_t                padding_;
   std::int32_t                startTime_; // Minutes since epoch
   std::int32_t                endTime_;   // Minutes since epoch
};

static constexpr std::size_t kPackedFillVertexSize = 28;
static_assert(sizeof(PackedFillVertex) == kPackedFillVertexSize);

/// IEEE 754 binary32 -> binary16 conversion constants
namespace half_float
{
static constexpr std::uint32_t kSignShift         = 16u;
static constexpr std::uint32_t kSignMask          = 0x8000u;
static constexpr std::uint32_t kMagnitudeMask     = 0x7FFFFFFFu;
static constexpr std::uint32_t kFloatInfinity     = 0x7F800000u;
static constexpr std::uint32_t kHalfInfinity      = 0x7C00u;
static constexpr std::uint32_t kHalfQuietNaN      = 0x0200u;
static constexpr std::uint32_t kHalfOverflow      = 0x477FF000u;
static constexpr std::uint32_t kHalfMinNormal     = 0x38800000u;
static constexpr std::uint32_t kHalfMinSubnormal  = 0x33000000u;
static constexpr std::uint32_t kExponentBiasDelta = 0x38000000u;
static constexpr std::uint32_t kMantissaShift     = 13u;
static constexpr std::uint32_t kRoundMask         = 0x1FFFu;
static constexpr std::uint32_t kRoundHalfway      = 0x1000u;
} // namespace half_float

/// Converts a float to IEEE 754 binary16 bits (round to nearest even).
[[nodiscard]] inline std::uint16_t FloatToHalfBits(float value)
{
   const auto          bits = std::bit_cast<std::uint32_t>(value);
   const std::uint32_t sign =
      (bits >> half_float::kSignShift) & half_float::kSignMask;
   const std::uint32_t mag = bits & half_float::kMagnitudeMask;

   if (mag >= half_float::kFloatInfinity)
   {
      // Infinity or NaN
      return static_cast<std::uint16_t>(
         sign | half_float::kHalfInfinity |
         (mag > half_float::kFloatInfinity ? half_float::kHalfQuietNaN : 0u));
   }
   if (mag >= half_float::kHalfOverflow)
   {
      // Rounds to or overflows past the largest half: infinity
      return static_cast<std::uint16_t>(sign | half_float::kHalfInfinity);
   }
   if (mag < half_float::kHalfMinNormal)
   {
      // Subnormal half or zero
      if (mag < half_float::kHalfMinSubnormal)
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
   std::uint32_t half =
      ((mag - half_float::kExponentBiasDelta) >> half_float::kMantissaShift);
   const std::uint32_t rem = mag & half_float::kRoundMask;
   if (rem > half_float::kRoundHalfway ||
       (rem == half_float::kRoundHalfway && (half & 1u) != 0u))
   {
      ++half;
   }
   return static_cast<std::uint16_t>(sign | half);
}

[[nodiscard]] inline std::int16_t ClampThreshold(std::int32_t threshold)
{
   return static_cast<std::int16_t>(
      std::clamp<std::int32_t>(threshold,
                               std::numeric_limits<std::int16_t>::min(),
                               std::numeric_limits<std::int16_t>::max()));
}

static constexpr double kMaxByte     = 255.0;
static constexpr long   kMaxByteLong = 255L;

[[nodiscard]] inline std::uint8_t NormalizedByte(double value)
{
   return static_cast<std::uint8_t>(
      std::clamp(std::lround(value * kMaxByte), 0L, kMaxByteLong));
}

/// Immutable, shareable vertex buffer. Every pane has its own placefile
/// layer, but the geometry is the same for all of them.
template<typename Vertex>
using SharedBuffer = std::shared_ptr<const std::vector<Vertex>>;

/**
 * Takes ownership of freshly built geometry and returns a shared buffer. If an
 * identical buffer is already alive (e.g. built by another pane for the same
 * placefile), that one is returned and the new vertices are freed, so N panes
 * hold one CPU copy rather than N. Equality is by content, so it can never
 * return a stale buffer.
 */
template<typename Vertex>
[[nodiscard]] SharedBuffer<Vertex> InternBuffer(std::vector<Vertex>&& vertices)
{
   static constexpr std::size_t kShrinkSlackDivisor = 16;

   static std::mutex                                            mutex;
   static std::vector<std::weak_ptr<const std::vector<Vertex>>> live;

   if (vertices.capacity() - vertices.size() >
       vertices.size() / kShrinkSlackDivisor)
   {
      vertices.shrink_to_fit();
   }

   const std::scoped_lock lock {mutex};

   std::erase_if(live, [](const auto& w) { return w.expired(); });

   for (const auto& w : live)
   {
      if (auto existing = w.lock();
          existing != nullptr && existing->size() == vertices.size() &&
          std::memcmp(existing->data(),
                      vertices.data(),
                      vertices.size() * sizeof(Vertex)) == 0)
      {
         return existing;
      }
   }

   auto buffer =
      std::make_shared<const std::vector<Vertex>>(std::move(vertices));
   live.push_back(buffer);
   return buffer;
}

} // namespace scwx::qt::gl::draw
