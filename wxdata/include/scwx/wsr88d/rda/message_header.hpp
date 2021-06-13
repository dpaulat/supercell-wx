#include <cstdint>
#include <memory>

namespace scwx
{
namespace wsr88d
{
namespace rda
{

class MessageHeaderImpl;

class MessageHeader
{
public:
   explicit MessageHeader();
   ~MessageHeader();

   MessageHeader(const MessageHeader&) = delete;
   MessageHeader& operator=(const MessageHeader&) = delete;

   MessageHeader(MessageHeader&&) noexcept;
   MessageHeader& operator=(MessageHeader&&);

   uint16_t message_size() const;
   uint8_t  rda_redundant_channel() const;
   uint8_t  message_type() const;
   uint16_t id_sequence_number() const;
   uint16_t julian_date() const;
   uint32_t milliseconds_of_day() const;
   uint16_t number_of_message_segments() const;
   uint16_t message_segment_number() const;

   bool Parse(std::istream& is);

   static const size_t SIZE = 16u;

private:
   std::unique_ptr<MessageHeaderImpl> p;
};

} // namespace rda
} // namespace wsr88d
} // namespace scwx
