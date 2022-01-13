#pragma once

#include <scwx/wsr88d/message.hpp>

#include <cstdint>
#include <memory>

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

class TabularAlphanumericBlockImpl;

class TabularAlphanumericBlock : public Message
{
public:
   explicit TabularAlphanumericBlock();
   ~TabularAlphanumericBlock();

   TabularAlphanumericBlock(const TabularAlphanumericBlock&) = delete;
   TabularAlphanumericBlock&
   operator=(const TabularAlphanumericBlock&) = delete;

   TabularAlphanumericBlock(TabularAlphanumericBlock&&) noexcept;
   TabularAlphanumericBlock& operator=(TabularAlphanumericBlock&&) noexcept;

   int16_t block_divider() const;

   size_t data_size() const override;

   bool Parse(std::istream& is);

   static constexpr size_t SIZE = 102u;

private:
   std::unique_ptr<TabularAlphanumericBlockImpl> p;
};

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
