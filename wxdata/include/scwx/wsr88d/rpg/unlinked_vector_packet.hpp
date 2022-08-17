#pragma once

#include <scwx/wsr88d/rpg/packet.hpp>

#include <cstdint>
#include <memory>
#include <optional>

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

class UnlinkedVectorPacketImpl;

class UnlinkedVectorPacket : public Packet
{
public:
   explicit UnlinkedVectorPacket();
   ~UnlinkedVectorPacket();

   UnlinkedVectorPacket(const UnlinkedVectorPacket&) = delete;
   UnlinkedVectorPacket& operator=(const UnlinkedVectorPacket&) = delete;

   UnlinkedVectorPacket(UnlinkedVectorPacket&&) noexcept;
   UnlinkedVectorPacket& operator=(UnlinkedVectorPacket&&) noexcept;

   uint16_t                packet_code() const override;
   uint16_t                length_of_block() const;
   std::optional<uint16_t> value_of_vector() const;

   size_t data_size() const override;

   bool Parse(std::istream& is) override;

   static std::shared_ptr<UnlinkedVectorPacket> Create(std::istream& is);

private:
   std::unique_ptr<UnlinkedVectorPacketImpl> p;
};

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
