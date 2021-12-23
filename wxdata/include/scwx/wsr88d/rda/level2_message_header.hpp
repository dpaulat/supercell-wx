#pragma once

#include <cstdint>
#include <memory>

namespace scwx
{
namespace wsr88d
{
namespace rda
{

class Level2MessageHeaderImpl;

class Level2MessageHeader
{
public:
   explicit Level2MessageHeader();
   ~Level2MessageHeader();

   Level2MessageHeader(const Level2MessageHeader&) = delete;
   Level2MessageHeader& operator=(const Level2MessageHeader&) = delete;

   Level2MessageHeader(Level2MessageHeader&&) noexcept;
   Level2MessageHeader& operator=(Level2MessageHeader&&) noexcept;

   uint16_t message_size() const;
   uint8_t  rda_redundant_channel() const;
   uint8_t  message_type() const;
   uint16_t id_sequence_number() const;
   uint16_t julian_date() const;
   uint32_t milliseconds_of_day() const;
   uint16_t number_of_message_segments() const;
   uint16_t message_segment_number() const;

   void set_message_size(uint16_t messageSize);

   bool Parse(std::istream& is);

   static const size_t SIZE = 16u;

private:
   std::unique_ptr<Level2MessageHeaderImpl> p;
};

} // namespace rda
} // namespace wsr88d
} // namespace scwx
