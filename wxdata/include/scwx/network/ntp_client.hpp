#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <string_view>

namespace scwx::network
{

/**
 * @brief NTP Client
 */
class NtpClient
{
public:
   explicit NtpClient();
   ~NtpClient();

   NtpClient(const NtpClient&)            = delete;
   NtpClient& operator=(const NtpClient&) = delete;

   NtpClient(NtpClient&&) noexcept;
   NtpClient& operator=(NtpClient&&) noexcept;

   bool error();

   [[nodiscard]] std::chrono::system_clock::duration time_offset() const;

   void Start();
   void Stop();

   void        Open(std::string_view host, std::string_view service);
   void        OpenCurrentServer();
   void        Poll();
   std::string RotateServer();
   void        RunOnce();

   void WaitForInitialOffset();

   static std::shared_ptr<NtpClient> Instance();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::network
