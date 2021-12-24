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

class Vector2DImpl;

class Vector2D : public Message
{
public:
   explicit Vector2D();
   ~Vector2D();

   Vector2D(const Vector2D&) = delete;
   Vector2D& operator=(const Vector2D&) = delete;

   Vector2D(Vector2D&&) noexcept;
   Vector2D& operator=(Vector2D&&) noexcept;

   int16_t start_i() const;
   int16_t start_j() const;
   int16_t end_i() const;
   int16_t end_j() const;

   size_t data_size() const override;

   bool Parse(std::istream& is) override;

   static constexpr size_t SIZE = 8u;

private:
   std::unique_ptr<Vector2DImpl> p;
};

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
