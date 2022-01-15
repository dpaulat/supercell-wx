#pragma once

#include <scwx/awips/message.hpp>
#include <scwx/wsr88d/rda/level2_message_header.hpp>

#ifdef WIN32
#   include <WinSock2.h>
#else
#   include <arpa/inet.h>
#endif

namespace scwx
{
namespace wsr88d
{
namespace rda
{

class Level2MessageImpl;

class Level2Message : public awips::Message
{
protected:
   explicit Level2Message();

   Level2Message(const Level2Message&) = delete;
   Level2Message& operator=(const Level2Message&) = delete;

   Level2Message(Level2Message&&) noexcept;
   Level2Message& operator=(Level2Message&&) noexcept;

public:
   virtual ~Level2Message();

   size_t data_size() const override;

   const Level2MessageHeader& header() const;

   void set_header(Level2MessageHeader&& header);

   static constexpr double ANGLE_DATA_SCALE      = 0.005493125;
   static constexpr double AZ_EL_RATE_DATA_SCALE = 0.001373291015625;

private:
   std::unique_ptr<Level2MessageImpl> p;
};

} // namespace rda
} // namespace wsr88d
} // namespace scwx
