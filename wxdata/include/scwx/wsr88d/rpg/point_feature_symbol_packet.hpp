#pragma once

#include <scwx/wsr88d/rpg/special_graphic_symbol_packet.hpp>

#include <cstdint>
#include <memory>

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

class PointFeatureSymbolPacketImpl;

class PointFeatureSymbolPacket : public SpecialGraphicSymbolPacket
{
public:
   explicit PointFeatureSymbolPacket();
   ~PointFeatureSymbolPacket();

   PointFeatureSymbolPacket(const PointFeatureSymbolPacket&) = delete;
   PointFeatureSymbolPacket&
   operator=(const PointFeatureSymbolPacket&) = delete;

   PointFeatureSymbolPacket(PointFeatureSymbolPacket&&) noexcept;
   PointFeatureSymbolPacket& operator=(PointFeatureSymbolPacket&&) noexcept;

   int16_t  i_position(size_t i) const;
   int16_t  j_position(size_t i) const;
   uint16_t point_feature_type(size_t i) const;
   uint16_t point_feature_attribute(size_t i) const;

   size_t RecordCount() const override;

   static std::shared_ptr<PointFeatureSymbolPacket> Create(std::istream& is);

protected:
   size_t MinBlockLength() const override;
   size_t MaxBlockLength() const override;

   bool ParseData(std::istream& is) override;

private:
   std::unique_ptr<PointFeatureSymbolPacketImpl> p;
};

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
