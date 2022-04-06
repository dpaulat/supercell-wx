#pragma once

#include <scwx/awips/message.hpp>
#include <scwx/wsr88d/rpg/packet.hpp>

#include <cstdint>
#include <memory>

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

class ProductSymbologyBlockImpl;

class ProductSymbologyBlock : public awips::Message
{
public:
   explicit ProductSymbologyBlock();
   ~ProductSymbologyBlock();

   ProductSymbologyBlock(const ProductSymbologyBlock&) = delete;
   ProductSymbologyBlock& operator=(const ProductSymbologyBlock&) = delete;

   ProductSymbologyBlock(ProductSymbologyBlock&&) noexcept;
   ProductSymbologyBlock& operator=(ProductSymbologyBlock&&) noexcept;

   int16_t  block_divider() const;
   uint16_t number_of_layers() const;

   std::vector<std::shared_ptr<Packet>> packet_list(uint16_t i) const;

   size_t data_size() const override;

   bool Parse(std::istream& is);

   static constexpr size_t SIZE = 102u;

private:
   std::unique_ptr<ProductSymbologyBlockImpl> p;
};

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
