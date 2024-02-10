#pragma once

#include <scwx/wsr88d/rpg/packet.hpp>

#include <cstdint>
#include <memory>
#include <optional>

#include <units/length.h>

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

class LinkedVectorPacketImpl;

class LinkedVectorPacket : public Packet
{
public:
   explicit LinkedVectorPacket();
   ~LinkedVectorPacket();

   LinkedVectorPacket(const LinkedVectorPacket&)            = delete;
   LinkedVectorPacket& operator=(const LinkedVectorPacket&) = delete;

   LinkedVectorPacket(LinkedVectorPacket&&) noexcept;
   LinkedVectorPacket& operator=(LinkedVectorPacket&&) noexcept;

   std::uint16_t                packet_code() const override;
   std::uint16_t                length_of_block() const;
   std::optional<std::uint16_t> value_of_vector() const;

   std::int16_t              start_i() const;
   std::int16_t              start_j() const;
   std::vector<std::int16_t> end_i() const;
   std::vector<std::int16_t> end_j() const;

   units::kilometers<double>              start_i_km() const;
   units::kilometers<double>              start_j_km() const;
   std::vector<units::kilometers<double>> end_i_km() const;
   std::vector<units::kilometers<double>> end_j_km() const;

   std::size_t data_size() const override;

   bool Parse(std::istream& is) override;

   static std::shared_ptr<LinkedVectorPacket> Create(std::istream& is);

private:
   std::unique_ptr<LinkedVectorPacketImpl> p;
};

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
