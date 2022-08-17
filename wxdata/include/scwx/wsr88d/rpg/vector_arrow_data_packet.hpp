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

class VectorArrowDataPacketImpl;

class VectorArrowDataPacket : public Packet
{
public:
   explicit VectorArrowDataPacket();
   ~VectorArrowDataPacket();

   VectorArrowDataPacket(const VectorArrowDataPacket&) = delete;
   VectorArrowDataPacket& operator=(const VectorArrowDataPacket&) = delete;

   VectorArrowDataPacket(VectorArrowDataPacket&&) noexcept;
   VectorArrowDataPacket& operator=(VectorArrowDataPacket&&) noexcept;

   uint16_t packet_code() const override;
   uint16_t length_of_block() const;

   size_t data_size() const override;

   bool Parse(std::istream& is) override;

   static std::shared_ptr<VectorArrowDataPacket> Create(std::istream& is);

private:
   std::unique_ptr<VectorArrowDataPacketImpl> p;
};

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
