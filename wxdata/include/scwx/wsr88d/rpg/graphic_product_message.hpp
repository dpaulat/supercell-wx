#pragma once

#include <scwx/wsr88d/rpg/graphic_alphanumeric_block.hpp>
#include <scwx/wsr88d/rpg/level3_message.hpp>
#include <scwx/wsr88d/rpg/product_description_block.hpp>
#include <scwx/wsr88d/rpg/product_symbology_block.hpp>
#include <scwx/wsr88d/rpg/tabular_alphanumeric_block.hpp>

#include <cstdint>
#include <memory>

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

class GraphicProductMessageImpl;

class GraphicProductMessage : public Level3Message
{
public:
   explicit GraphicProductMessage();
   ~GraphicProductMessage();

   GraphicProductMessage(const GraphicProductMessage&) = delete;
   GraphicProductMessage& operator=(const GraphicProductMessage&) = delete;

   GraphicProductMessage(GraphicProductMessage&&) noexcept;
   GraphicProductMessage& operator=(GraphicProductMessage&&) noexcept;

   std::shared_ptr<ProductDescriptionBlock>  description_block() const;
   std::shared_ptr<ProductSymbologyBlock>    symbology_block() const;
   std::shared_ptr<GraphicAlphanumericBlock> graphic_block() const;
   std::shared_ptr<TabularAlphanumericBlock> tabular_block() const;

   bool Parse(std::istream& is) override;

   static std::shared_ptr<GraphicProductMessage>
   Create(Level3MessageHeader&& header, std::istream& is);

private:
   std::unique_ptr<GraphicProductMessageImpl> p;
};

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
