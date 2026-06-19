#include <scwx/network/ntp_client.hpp>
#include <scwx/types/ntp_types.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/threads.hpp>

#include <condition_variable>
#include <mutex>
#include <optional>

#include <boost/asio/ip/udp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/use_future.hpp>
#include <fmt/chrono.h>

namespace scwx::network
{

static const std::string logPrefix_ = "scwx::network::ntp_client";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr std::size_t kReceiveBufferSize_ {48u};

// Reasonable min/max values for polling intervals. We don't want to poll too
// quickly and upset the server, but we don't want to poll too slowly in the
// event of a time jump.
static constexpr std::uint32_t kMinPollInterval_ = 6u; // 2^6 = 64 seconds
static constexpr std::uint32_t kMaxPollInterval_ = 9u; // 2^9 = 512 seconds

class NtpTimestamp
{
public:
   // NTP epoch: January 1, 1900
   // Unix epoch: January 1, 1970
   // Difference = 70 years = 2,208,988,800 seconds
   static constexpr std::uint32_t kNtpToUnixOffset_ = 2208988800UL;

   // NTP fractional part represents 1/2^32 of a second
   static constexpr std::uint64_t kFractionalMultiplier_ = 0x100000000ULL;

   static constexpr std::uint64_t _1e9 = 1000000000ULL;

   std::uint32_t seconds_ {0};
   std::uint32_t fraction_ {0};

   explicit NtpTimestamp() = default;
   explicit NtpTimestamp(std::uint32_t seconds, std::uint32_t fraction) :
       seconds_ {seconds}, fraction_ {fraction}
   {
   }
   ~NtpTimestamp() = default;

   NtpTimestamp(const NtpTimestamp&)            = default;
   NtpTimestamp& operator=(const NtpTimestamp&) = default;
   NtpTimestamp(NtpTimestamp&&)                 = default;
   NtpTimestamp& operator=(NtpTimestamp&&)      = default;

   template<typename Clock = std::chrono::system_clock>
   [[nodiscard]] std::chrono::time_point<Clock> ToTimePoint() const
   {
      // Convert NTP seconds to Unix seconds
      // Don't cast to a larger type to account for rollover, and this should
      // work until 2106
      const std::uint32_t unixSeconds = seconds_ - kNtpToUnixOffset_;

      // Convert NTP fraction to nanoseconds
      const auto nanoseconds =
         static_cast<std::uint64_t>(fraction_) * _1e9 / kFractionalMultiplier_;

      return std::chrono::time_point<Clock>(
         std::chrono::duration_cast<typename Clock::duration>(
            std::chrono::seconds {unixSeconds} +
            std::chrono::nanoseconds {nanoseconds}));
   }

   template<typename Clock = std::chrono::system_clock>
   static NtpTimestamp FromTimePoint(std::chrono::time_point<Clock> timePoint)
   {
      // Convert to duration since Unix epoch
      const auto unixDuration = timePoint.time_since_epoch();

      // Extract seconds and nanoseconds
      const auto unixSeconds =
         std::chrono::duration_cast<std::chrono::seconds>(unixDuration);
      const auto nanoseconds =
         std::chrono::duration_cast<std::chrono::nanoseconds>(unixDuration -
                                                              unixSeconds);

      // Convert Unix seconds to NTP seconds
      const auto ntpSeconds =
         static_cast<std::uint32_t>(unixSeconds.count() + kNtpToUnixOffset_);

      // Convert nanoseconds to NTP fractional seconds
      const auto ntpFraction = static_cast<std::uint32_t>(
         nanoseconds.count() * kFractionalMultiplier_ / _1e9);

      return NtpTimestamp(ntpSeconds, ntpFraction);
   }
};

class NtpClient::Impl
{
public:
   explicit Impl();
   ~Impl();
   Impl(const Impl&)            = delete;
   Impl& operator=(const Impl&) = delete;
   Impl(Impl&&)                 = delete;
   Impl& operator=(Impl&&)      = delete;

   void        Open(std::string_view host, std::string_view service);
   void        OpenCurrentServer();
   void        Poll();
   void        ReceivePacket(std::size_t length);
   std::string RotateServer();
   void        Run();
   void        RunOnce();

   void FinishInitialization();

   boost::asio::thread_pool threadPool_ {2u};

   boost::asio::steady_timer pollTimer_ {threadPool_};
   std::uint32_t             pollInterval_ {kMinPollInterval_};

   bool enabled_ {true};
   bool error_ {false};
   bool disableServer_ {false};
   bool rotateServer_ {false};

   std::mutex              initializationMutex_ {};
   std::condition_variable initializationCondition_ {};
   std::atomic<bool>       initialized_ {false};

   types::ntp::NtpPacket transmitPacket_ {};

   boost::asio::ip::udp::socket                  socket_ {threadPool_};
   std::optional<boost::asio::ip::udp::endpoint> serverEndpoint_ {};
   std::array<std::uint8_t, kReceiveBufferSize_> receiveBuffer_ {};

   std::chrono::system_clock::duration timeOffset_ {};

   const std::vector<std::string> serverList_ {"time.nist.gov",
                                               "time.cloudflare.com",
                                               "pool.ntp.org",
                                               "time.aws.com",
                                               "time.windows.com",
                                               "time.apple.com"};
   std::vector<std::string>       disabledServers_ {};

   std::vector<std::string>::const_iterator currentServer_ =
      serverList_.begin();
};

NtpClient::NtpClient() : p(std::make_unique<Impl>()) {}
NtpClient::~NtpClient() = default;

NtpClient::NtpClient(NtpClient&&) noexcept            = default;
NtpClient& NtpClient::operator=(NtpClient&&) noexcept = default;

NtpClient::Impl::Impl()
{
   using namespace std::chrono_literals;

   const auto now =
      std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now());

   // The NTP timestamp will overflow in 2036. Overflow is handled in such a way
   // that dates prior to 1970 result in a Unix timestamp after 2036. Additional
   // handling for the year 2106 and subsequent eras is required.
   static constexpr auto kMaxYear_ = 2106y;

   enabled_ = now < kMaxYear_ / 1 / 1;

   transmitPacket_.fields.vn   = 3; // Version
   transmitPacket_.fields.mode = 3; // Client (3)

   // If the NTP client is enabled, wait until the first refresh to consider
   // "initialized". Otherwise, mark as initialized immediately to prevent a
   // deadlock.
   if (!enabled_)
   {
      initialized_ = true;
   }
}

NtpClient::Impl::~Impl()
{
   threadPool_.join();
}

bool NtpClient::error()
{
   const bool returnValue = p->error_;
   p->error_              = false;
   return returnValue;
}

std::chrono::system_clock::duration NtpClient::time_offset() const
{
   return p->timeOffset_;
}

void NtpClient::Start()
{
   if (p->enabled_)
   {
      boost::asio::post(p->threadPool_, [this]() { p->Run(); });
   }
}

void NtpClient::Stop()
{
   p->enabled_ = false;
   p->socket_.cancel();
   p->pollTimer_.cancel();
   p->threadPool_.join();
}

void NtpClient::Open(std::string_view host, std::string_view service)
{
   p->Open(host, service);
}

void NtpClient::OpenCurrentServer()
{
   p->OpenCurrentServer();
}

void NtpClient::Poll()
{
   p->Poll();
}

std::string NtpClient::RotateServer()
{
   return p->RotateServer();
}

void NtpClient::RunOnce()
{
   p->RunOnce();
}

void NtpClient::Impl::Open(std::string_view host, std::string_view service)
{
   boost::asio::ip::udp::resolver resolver(threadPool_);
   boost::system::error_code      ec;

   auto results = resolver.resolve(host, service, ec);
   if (ec.value() == boost::system::errc::success && !results.empty())
   {
      logger_->info("Using NTP server: {}", host);
      serverEndpoint_ = *results.begin();
      socket_.open(serverEndpoint_->protocol());
   }
   else
   {
      serverEndpoint_ = std::nullopt;
      logger_->warn("Could not resolve host {}: {}", host, ec.message());
      rotateServer_ = true;
   }
}

void NtpClient::Impl::OpenCurrentServer()
{
   Open(*currentServer_, "123");
}

void NtpClient::Impl::Poll()
{
   using namespace std::chrono_literals;

   static constexpr auto kTimeout_ = 5s;

   if (!serverEndpoint_.has_value())
   {
      logger_->error("Server endpoint not set");
      error_ = true;
      return;
   }

   try
   {
      const auto originTimestamp =
         NtpTimestamp::FromTimePoint(std::chrono::system_clock::now());
      transmitPacket_.txTm_s = ntohl(originTimestamp.seconds_);
      transmitPacket_.txTm_f = ntohl(originTimestamp.fraction_);

      const std::size_t transmitPacketSize = sizeof(transmitPacket_);
      // Send NTP request
      socket_.send_to(boost::asio::buffer(&transmitPacket_, transmitPacketSize),
                      *serverEndpoint_);

      // Receive NTP response
      auto future =
         socket_.async_receive_from(boost::asio::buffer(receiveBuffer_),
                                    *serverEndpoint_,
                                    boost::asio::use_future);
      std::size_t bytesReceived = 0;

      switch (future.wait_for(kTimeout_))
      {
      case std::future_status::ready:
         bytesReceived = future.get();
         ReceivePacket(bytesReceived);
         break;

      case std::future_status::timeout:
      case std::future_status::deferred:
         logger_->warn("Timeout waiting for NTP response");
         socket_.cancel();
         error_ = true;
         break;
      }
   }
   catch (const std::exception& ex)
   {
      logger_->error("Error polling: {}", ex.what());
      error_ = true;
   }
}

void NtpClient::Impl::ReceivePacket(std::size_t length)
{
   if (length >= sizeof(types::ntp::NtpPacket))
   {
      const auto destinationTime = std::chrono::system_clock::now();

      const auto packet = types::ntp::NtpPacket::Parse(receiveBuffer_);

      if (packet.stratum == 0)
      {
         const std::uint32_t refId = ntohl(packet.refId);
         const std::string   kod =
            std::string(reinterpret_cast<const char*>(&refId), 4);

         logger_->warn("KoD packet received: {}", kod);

         if (kod == "DENY" || kod == "RSTR")
         {
            // The client MUST demobilize any associations to that server and
            // stop sending packets to that server
            disableServer_ = true;
         }
         else if (kod == "RATE")
         {
            // The client MUST immediately reduce its polling interval to that
            // server and continue to reduce it each time it receives a RATE
            // kiss code
            if (pollInterval_ < kMaxPollInterval_)
            {
               ++pollInterval_;
            }
            else
            {
               // The server wants us to reduce the polling interval lower than
               // what we deem useful. Move to the next server.
               rotateServer_ = true;
            }
         }

         // Consider a KoD packet an error
         error_ = true;
      }
      else
      {
         const auto originTimestamp =
            NtpTimestamp(packet.origTm_s, packet.origTm_f);
         const auto receiveTimestamp =
            NtpTimestamp(packet.rxTm_s, packet.rxTm_f);
         const auto transmitTimestamp =
            NtpTimestamp(packet.txTm_s, packet.txTm_f);

         const auto originTime   = originTimestamp.ToTimePoint();
         const auto receiveTime  = receiveTimestamp.ToTimePoint();
         const auto transmitTime = transmitTimestamp.ToTimePoint();

         const auto& t0 = originTime;
         const auto& t1 = receiveTime;
         const auto& t2 = transmitTime;
         const auto& t3 = destinationTime;

         // Update time offset
         timeOffset_ = ((t1 - t0) + (t2 - t3)) / 2;

         logger_->debug("Time offset updated: {:%jd %T}", timeOffset_);
      }
   }
   else
   {
      logger_->warn("Received too few bytes: {}", length);
      error_ = true;
   }
}

std::string NtpClient::Impl::RotateServer()
{
   socket_.close();

   bool newServerFound = false;

   // Save the current server
   const auto oldServer = currentServer_;

   while (!newServerFound)
   {
      // Increment the current server
      ++currentServer_;

      // If we are at the end of the list, start over at the beginning
      if (currentServer_ == serverList_.end())
      {
         currentServer_ = serverList_.begin();
      }

      // If we have reached the end of the list, give up
      if (currentServer_ == oldServer)
      {
         enabled_ = false;
         break;
      }

      // If the current server is disabled, continue searching
      while (std::find(disabledServers_.cbegin(),
                       disabledServers_.cend(),
                       *currentServer_) != disabledServers_.cend())
      {
         continue;
      }

      // A new server has been found
      newServerFound = true;
   }

   pollInterval_ = kMinPollInterval_;
   rotateServer_ = false;

   return *currentServer_;
}

void NtpClient::Impl::Run()
{
   RunOnce();

   if (enabled_)
   {
      const std::chrono::seconds pollIntervalSeconds {1u << pollInterval_};
      pollTimer_.expires_after(pollIntervalSeconds);
      pollTimer_.async_wait(
         [this](const boost::system::error_code& e)
         {
            if (e == boost::asio::error::operation_aborted)
            {
               logger_->debug("Poll timer cancelled");
            }
            else if (e != boost::system::errc::success)
            {
               logger_->warn("Poll timer error: {}", e.message());
            }
            else
            {
               try
               {
                  Run();
               }
               catch (const std::exception& ex)
               {
                  logger_->error(ex.what());
               }
            }
         });
   }
}

void NtpClient::Impl::RunOnce()
{
   if (disableServer_)
   {
      // Disable the current server
      disabledServers_.push_back(*currentServer_);

      // Disable the NTP client if all servers are disabled
      enabled_ = disabledServers_.size() == serverList_.size();

      if (!enabled_)
      {
         error_ = true;
      }

      disableServer_ = false;
      rotateServer_  = enabled_;
   }

   if (!enabled_ && socket_.is_open())
   {
      // Sockets should be closed if the client is disabled
      socket_.close();
   }

   if (rotateServer_)
   {
      // Rotate the server if requested
      RotateServer();
   }

   if (enabled_ && !socket_.is_open())
   {
      // Open the current server if it is not open
      OpenCurrentServer();
   }

   if (socket_.is_open())
   {
      // Send an NTP message to determine the current time offset
      Poll();
   }
   else if (enabled_)
   {
      // Did not poll this frame
      error_ = true;
   }

   FinishInitialization();
}

void NtpClient::Impl::FinishInitialization()
{
   if (!initialized_)
   {
      // Set initialized to true
      std::unique_lock lock(initializationMutex_);
      initialized_ = true;
      lock.unlock();

      // Notify any threads waiting for initialization
      initializationCondition_.notify_all();
   }
}

void NtpClient::WaitForInitialOffset()
{
   std::unique_lock lock(p->initializationMutex_);

   // While not yet initialized
   while (!p->initialized_)
   {
      // Wait for initialization
      p->initializationCondition_.wait(lock);
   }
}

std::shared_ptr<NtpClient> NtpClient::Instance()
{
   static std::weak_ptr<NtpClient> ntpClientReference_ {};
   static std::mutex               instanceMutex_ {};

   const std::unique_lock lock(instanceMutex_);

   std::shared_ptr<NtpClient> ntpClient = ntpClientReference_.lock();

   if (ntpClient == nullptr)
   {
      ntpClient           = std::make_shared<NtpClient>();
      ntpClientReference_ = ntpClient;
   }

   return ntpClient;
}

} // namespace scwx::network
