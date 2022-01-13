#pragma once

#include <scwx/wsr88d/rpg/level3_message.hpp>

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

class Level3MessageFactory
{
private:
   explicit Level3MessageFactory() = delete;
   ~Level3MessageFactory()         = delete;

   Level3MessageFactory(const Level3MessageFactory&) = delete;
   Level3MessageFactory& operator=(const Level3MessageFactory&) = delete;

   Level3MessageFactory(Level3MessageFactory&&) noexcept = delete;
   Level3MessageFactory& operator=(Level3MessageFactory&&) noexcept = delete;

public:
   static std::shared_ptr<Level3Message> Create(std::istream& is);
};

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
