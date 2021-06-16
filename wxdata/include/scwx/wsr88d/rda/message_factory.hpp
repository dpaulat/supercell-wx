#pragma once

#include <scwx/wsr88d/rda/message.hpp>

namespace scwx
{
namespace wsr88d
{
namespace rda
{

struct MessageInfo
{
   std::unique_ptr<Message> message;
   bool                     headerValid;
   bool                     messageValid;

   MessageInfo() : message(nullptr), headerValid(false), messageValid(false) {}
};

class MessageFactory
{
private:
   explicit MessageFactory() = delete;
   ~MessageFactory()         = delete;

   MessageFactory(const Message&) = delete;
   MessageFactory& operator=(const MessageFactory&) = delete;

   MessageFactory(MessageFactory&&) noexcept = delete;
   MessageFactory& operator=(MessageFactory&&) noexcept = delete;

public:
   static MessageInfo Create(std::istream& is);
};

} // namespace rda
} // namespace wsr88d
} // namespace scwx
