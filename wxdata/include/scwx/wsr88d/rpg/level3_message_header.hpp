#pragma once

#include <cstdint>
#include <memory>

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

class Level3MessageHeaderImpl;

class Level3MessageHeader
{
public:
   explicit Level3MessageHeader();
   ~Level3MessageHeader();

   Level3MessageHeader(const Level3MessageHeader&) = delete;
   Level3MessageHeader& operator=(const Level3MessageHeader&) = delete;

   Level3MessageHeader(Level3MessageHeader&&) noexcept;
   Level3MessageHeader& operator=(Level3MessageHeader&&) noexcept;

   int16_t  message_code() const;
   uint16_t date_of_message() const;
   uint32_t time_of_message() const;
   uint32_t length_of_message() const;
   uint16_t source_id() const;
   uint16_t destination_id() const;
   uint16_t number_blocks() const;

   bool Parse(std::istream& is);

   static constexpr size_t SIZE = 18u;

private:
   std::unique_ptr<Level3MessageHeaderImpl> p;
};

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
