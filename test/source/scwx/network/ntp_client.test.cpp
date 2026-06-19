#include <scwx/network/ntp_client.hpp>

#include <gtest/gtest.h>

namespace scwx::network
{

TEST(NtpClient, Poll)
{
   NtpClient client {};

   const std::string firstServer   = client.RotateServer();
   std::string       currentServer = firstServer;
   std::string       lastServer    = firstServer;
   bool              error         = false;

   do
   {
      client.RunOnce();
      error = client.error();

      EXPECT_EQ(error, false);

      // Loop until the current server repeats the first server, or fails to
      // rotate
      lastServer    = currentServer;
      currentServer = client.RotateServer();
   } while (currentServer != firstServer && currentServer != lastServer &&
            !error);
}

} // namespace scwx::network
