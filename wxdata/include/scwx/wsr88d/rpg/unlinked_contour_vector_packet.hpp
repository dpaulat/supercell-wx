#pragma once

#include <scwx/wsr88d/rpg/packet.hpp>

#include <cstdint>
#include <memory>

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

class UnlinkedContourVectorPacketImpl;

class UnlinkedContourVectorPacket : public Packet
{
public:
   explicit UnlinkedContourVectorPacket();
   ~UnlinkedContourVectorPacket();

   UnlinkedContourVectorPacket(const UnlinkedContourVectorPacket&) = delete;
   UnlinkedContourVectorPacket&
   operator=(const UnlinkedContourVectorPacket&) = delete;

   UnlinkedContourVectorPacket(UnlinkedContourVectorPacket&&) noexcept;
   UnlinkedContourVectorPacket&
   operator=(UnlinkedContourVectorPacket&&) noexcept;

   uint16_t packet_code() const override;
   uint16_t length_of_vectors() const;

   size_t data_size() const override;

   bool Parse(std::istream& is) override;

   static std::shared_ptr<UnlinkedContourVectorPacket> Create(std::istream& is);

private:
   std::unique_ptr<UnlinkedContourVectorPacketImpl> p;
};

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
