#pragma once

#include <scwx/wsr88d/rda/level2_message.hpp>

namespace scwx
{
namespace wsr88d
{
namespace rda
{

struct Level2MessageInfo
{
   std::shared_ptr<Level2Message> message;
   bool                           headerValid;
   bool                           messageValid;

   Level2MessageInfo() :
       message(nullptr), headerValid(false), messageValid(false)
   {
   }
};

class Level2MessageFactory
{
private:
   explicit Level2MessageFactory() = delete;
   ~Level2MessageFactory()         = delete;

   Level2MessageFactory(const Level2MessageFactory&)            = delete;
   Level2MessageFactory& operator=(const Level2MessageFactory&) = delete;

   Level2MessageFactory(Level2MessageFactory&&) noexcept            = delete;
   Level2MessageFactory& operator=(Level2MessageFactory&&) noexcept = delete;

public:
   struct Context;

   static std::shared_ptr<Context> CreateContext();
   static Level2MessageInfo        Create(std::istream&             is,
                                          std::shared_ptr<Context>& ctx);
};

} // namespace rda
} // namespace wsr88d
} // namespace scwx
