#pragma once

#include <scwx/wsr88d/rda/level2_message.hpp>

namespace scwx
{
namespace wsr88d
{
namespace rda
{

class ClutterFilterBypassMapImpl;

class ClutterFilterBypassMap : public Level2Message
{
public:
   explicit ClutterFilterBypassMap();
   ~ClutterFilterBypassMap();

   ClutterFilterBypassMap(const ClutterFilterBypassMap&) = delete;
   ClutterFilterBypassMap& operator=(const ClutterFilterBypassMap&) = delete;

   ClutterFilterBypassMap(ClutterFilterBypassMap&&) noexcept;
   ClutterFilterBypassMap& operator=(ClutterFilterBypassMap&&) noexcept;

   uint16_t map_generation_date() const;
   uint16_t map_generation_time() const;
   uint16_t number_of_elevation_segments() const;
   uint16_t range_bin(uint16_t e, uint16_t r, uint16_t b) const;

   bool Parse(std::istream& is);

   static std::shared_ptr<ClutterFilterBypassMap>
   Create(Level2MessageHeader&& header, std::istream& is);

   static constexpr size_t NUM_RADIALS          = 360u;
   static constexpr size_t NUM_RANGE_BINS       = 512u;
   static constexpr size_t NUM_CODED_RANGE_BINS = NUM_RANGE_BINS / 16u;

private:
   std::unique_ptr<ClutterFilterBypassMapImpl> p;
};

} // namespace rda
} // namespace wsr88d
} // namespace scwx
