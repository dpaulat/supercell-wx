#pragma once

#include <scwx/wsr88d/rpg/level3_message.hpp>
#include <scwx/wsr88d/rpg/product_description_block.hpp>
#include <scwx/wsr88d/rpg/tabular_alphanumeric_block.hpp>

#include <cstdint>
#include <memory>

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

class TabularProductMessageImpl;

class TabularProductMessage : public Level3Message
{
public:
   explicit TabularProductMessage();
   ~TabularProductMessage();

   TabularProductMessage(const TabularProductMessage&) = delete;
   TabularProductMessage& operator=(const TabularProductMessage&) = delete;

   TabularProductMessage(TabularProductMessage&&) noexcept;
   TabularProductMessage& operator=(TabularProductMessage&&) noexcept;

   std::shared_ptr<ProductDescriptionBlock>  description_block() const override;
   std::shared_ptr<TabularAlphanumericBlock> tabular_block() const;

   bool Parse(std::istream& is) override;

   static std::shared_ptr<TabularProductMessage>
   Create(Level3MessageHeader&& header, std::istream& is);

private:
   std::unique_ptr<TabularProductMessageImpl> p;
};

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
