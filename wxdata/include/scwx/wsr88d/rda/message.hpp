#pragma once

#include <scwx/wsr88d/rda/message_header.hpp>

namespace scwx
{
namespace wsr88d
{
namespace rda
{

class MessageImpl;

class Message
{
protected:
   explicit Message();

   Message(const Message&) = delete;
   Message& operator=(const Message&) = delete;

   Message(Message&&) noexcept;
   Message& operator=(Message&&) noexcept;

   bool ValidateSize(std::istream& is, size_t bytesRead) const;

public:
   virtual ~Message();

   const MessageHeader& header() const;

   void set_header(MessageHeader&& header);

   virtual bool Parse(std::istream& is) = 0;

private:
   std::unique_ptr<MessageImpl> p;
};

} // namespace rda
} // namespace wsr88d
} // namespace scwx
