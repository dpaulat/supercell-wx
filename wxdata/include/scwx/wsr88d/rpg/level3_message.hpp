#pragma once

#include <scwx/awips/message.hpp>
#include <scwx/wsr88d/rpg/level3_message_header.hpp>

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

class Level3MessageImpl;

class Level3Message : public awips::Message
{
protected:
   explicit Level3Message();

   Level3Message(const Level3Message&) = delete;
   Level3Message& operator=(const Level3Message&) = delete;

   Level3Message(Level3Message&&) noexcept;
   Level3Message& operator=(Level3Message&&) noexcept;

public:
   virtual ~Level3Message();

   size_t data_size() const override;

   const Level3MessageHeader& header() const;

   void set_header(Level3MessageHeader&& header);

private:
   std::unique_ptr<Level3MessageImpl> p;
};

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
