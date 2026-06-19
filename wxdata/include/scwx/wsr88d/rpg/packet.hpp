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

class Packet : public awips::Message
{
public:
   Packet(const Packet&)            = delete;
   Packet& operator=(const Packet&) = delete;

   virtual ~Packet();

   virtual uint16_t packet_code() const = 0;

protected:
   explicit Packet();

   Packet(Packet&&) noexcept;
   Packet& operator=(Packet&&) noexcept;
};

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
