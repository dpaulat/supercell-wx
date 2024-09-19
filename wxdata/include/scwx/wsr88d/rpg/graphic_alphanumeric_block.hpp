#pragma once

#include <scwx/awips/message.hpp>
#include <scwx/wsr88d/rpg/packet.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

class GraphicAlphanumericBlockImpl;

class GraphicAlphanumericBlock : public awips::Message
{
public:
   explicit GraphicAlphanumericBlock();
   ~GraphicAlphanumericBlock();

   GraphicAlphanumericBlock(const GraphicAlphanumericBlock&) = delete;
   GraphicAlphanumericBlock&
   operator=(const GraphicAlphanumericBlock&) = delete;

   GraphicAlphanumericBlock(GraphicAlphanumericBlock&&) noexcept;
   GraphicAlphanumericBlock& operator=(GraphicAlphanumericBlock&&) noexcept;

   int16_t block_divider() const;

   size_t data_size() const override;

   const std::vector<std::vector<std::shared_ptr<Packet>>>& page_list() const;

   bool Parse(std::istream& is) override;

   static constexpr size_t SIZE = 102u;

private:
   std::unique_ptr<GraphicAlphanumericBlockImpl> p;
};

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
