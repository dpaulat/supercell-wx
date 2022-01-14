#pragma once

#include <scwx/wsr88d/rpg/level3_message.hpp>
#include <scwx/wsr88d/rpg/product_description_block.hpp>

#include <cstdint>
#include <memory>

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

class RadarCodedMessageImpl;

class RadarCodedMessage : public Level3Message
{
public:
   explicit RadarCodedMessage();
   ~RadarCodedMessage();

   RadarCodedMessage(const RadarCodedMessage&) = delete;
   RadarCodedMessage& operator=(const RadarCodedMessage&) = delete;

   RadarCodedMessage(RadarCodedMessage&&) noexcept;
   RadarCodedMessage& operator=(RadarCodedMessage&&) noexcept;

   std::shared_ptr<ProductDescriptionBlock> description_block() const;

   bool Parse(std::istream& is) override;

   static std::shared_ptr<RadarCodedMessage>
   Create(Level3MessageHeader&& header, std::istream& is);

private:
   std::unique_ptr<RadarCodedMessageImpl> p;
};

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
