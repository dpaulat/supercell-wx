#pragma once

#include <scwx/awips/message.hpp>

#include <cstdint>
#include <memory>

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

   bool Parse(std::istream& is);

   static constexpr size_t SIZE = 102u;

private:
   std::unique_ptr<GraphicAlphanumericBlockImpl> p;
};

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
