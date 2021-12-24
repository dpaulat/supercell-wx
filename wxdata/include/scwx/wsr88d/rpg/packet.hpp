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

class Packet : public Message
{
protected:
   explicit Packet();

   Packet(const Packet&) = delete;
   Packet& operator=(const Packet&) = delete;

   Packet(Packet&&) noexcept;
   Packet& operator=(Packet&&) noexcept;

public:
   virtual ~Packet();

   virtual uint16_t packet_code() const = 0;
};

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
