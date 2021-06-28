#pragma once

#include <scwx/wsr88d/rda/message.hpp>

namespace scwx
{
namespace wsr88d
{
namespace rda
{

class ClutterFilterMapImpl;

class ClutterFilterMap : public Message
{
public:
   explicit ClutterFilterMap();
   ~ClutterFilterMap();

   ClutterFilterMap(const ClutterFilterMap&) = delete;
   ClutterFilterMap& operator=(const ClutterFilterMap&) = delete;

   ClutterFilterMap(ClutterFilterMap&&) noexcept;
   ClutterFilterMap& operator=(ClutterFilterMap&&) noexcept;

   uint16_t map_generation_date() const;
   uint16_t map_generation_time() const;
   uint16_t number_of_elevation_segments() const;
   uint16_t number_of_range_zones(uint16_t e, uint16_t a) const;
   uint16_t op_code(uint16_t e, uint16_t a, uint16_t z) const;
   uint16_t end_range(uint16_t e, uint16_t a, uint16_t z) const;

   bool Parse(std::istream& is);

   static std::shared_ptr<ClutterFilterMap> Create(MessageHeader&& header,
                                                   std::istream&   is);

   static const size_t NUM_AZIMUTH_SEGMENTS = 360u;

private:
   std::unique_ptr<ClutterFilterMapImpl> p;
};

} // namespace rda
} // namespace wsr88d
} // namespace scwx
