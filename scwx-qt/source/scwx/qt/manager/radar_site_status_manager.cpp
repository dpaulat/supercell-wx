#include <scwx/qt/config/radar_site.hpp>
#include <scwx/qt/manager/radar_site_status_manager.hpp>
#include <scwx/qt/types/radar_site_types.hpp>
#include <scwx/provider/nws_api_provider.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/time.hpp>

#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/thread_pool.hpp>

#if (__cpp_lib_chrono < 201907L)
#   include <date/date.h>
#endif

namespace scwx::qt::manager
{

static const std::string logPrefix_ =
   "scwx::qt::manager::radar_site_status_manager";
static const auto logger_ = util::Logger::Create(logPrefix_);

class RadarSiteStatusManager::Impl
{
public:
   explicit Impl(RadarSiteStatusManager* self) : self_ {self} {}
   ~Impl()                       = default;
   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   void Stop();
   void Run();
   void RunOnce();

   RadarSiteStatusManager* self_ {};

   boost::asio::thread_pool threadPool_ {1u};

   std::atomic<bool>         scheduled_ {false};
   std::mutex                scheduleMutex_ {};
   boost::asio::steady_timer scheduleTimer_ {threadPool_};

   std::unique_ptr<provider::NwsApiProvider> nwsApiProvider_ {
      std::make_unique<provider::NwsApiProvider>()};

   std::chrono::system_clock::time_point lastUpdate_ {};
};

RadarSiteStatusManager::RadarSiteStatusManager() :
    p(std::make_unique<Impl>(this))
{
}
RadarSiteStatusManager::~RadarSiteStatusManager() = default;

void RadarSiteStatusManager::Start()
{
   std::unique_lock lock {p->scheduleMutex_};
   if (p->scheduled_)
   {
      return;
   }
   p->scheduled_ = true;
   lock.unlock();

   boost::asio::post(p->threadPool_, [this]() { p->Run(); });
}

void RadarSiteStatusManager::Stop()
{
   p->Stop();
}

void RadarSiteStatusManager::Impl::Stop()
{
   std::unique_lock lock {scheduleMutex_};
   if (scheduled_)
   {
      scheduled_ = false;
      scheduleTimer_.cancel();
   }
   lock.unlock();

   nwsApiProvider_->Shutdown();

   threadPool_.stop();
   threadPool_.join();
}

void RadarSiteStatusManager::Impl::Run()
{
   using namespace std::chrono_literals;

   static constexpr auto kInterval = 1min;

   RunOnce();

   const std::unique_lock lock {scheduleMutex_};

   if (scheduled_)
   {
      scheduleTimer_.expires_after(kInterval);
      scheduleTimer_.async_wait(
         [this](const boost::system::error_code& e)
         {
            if (e == boost::system::errc::success)
            {
               Run();
            }
            else if (e == boost::asio::error::operation_aborted)
            {
               logger_->debug("Schedule timer cancelled");
            }
            else
            {
               logger_->warn("Schedule timer error: {}", e.message());
               scheduled_ = false;
            }
         });
   }
}

void RadarSiteStatusManager::Impl::RunOnce()
{
   const auto now      = scwx::util::time::now();
   const auto response = nwsApiProvider_->GetRadarStations();

   if (response.has_value())
   {
      const auto& result = response.value();
      for (auto& station : result)
      {
         types::RadarSiteStatus status = types::RadarSiteStatus::Unknown;
         std::chrono::system_clock::time_point lastReceived {};
         std::chrono::milliseconds             currentLatency {};

         if (station.latency_.has_value())
         {
            using namespace std::chrono;

#if (__cpp_lib_chrono < 201907L)
            using namespace date;
#endif

            auto& latency = station.latency_.value();

            // "YYYY-mm-ddTHH:MM:SS+00:00"
            static const std::string dateTimeFormat {"%Y-%m-%dT%H:%M:%S%Ez"};

            // Parse level 2 last receive time
            std::istringstream ssDateTime {latency.levelTwoLastReceivedTime_};
            std::chrono::system_clock::time_point dateTime;

            ssDateTime >> parse(dateTimeFormat, dateTime);

            if (!ssDateTime.fail())
            {
               lastReceived = dateTime;

               // Determine level 2 age
               static constexpr auto kDownThreshold    = 30min;
               static constexpr auto kWarningThreshold = 5min;

               auto age = now - dateTime;

               if (age > kDownThreshold)
               {
                  status = types::RadarSiteStatus::Down;
               }
               else if (age > kWarningThreshold)
               {
                  status = types::RadarSiteStatus::Warning;
               }
               else
               {
                  status = types::RadarSiteStatus::Up;
               }
            }
            else if (!latency.levelTwoLastReceivedTime_.empty())
            {
               logger_->warn("Could not parse last receive time for {}: {}",
                             station.id_,
                             latency.levelTwoLastReceivedTime_);
            }

            if ((status == types::RadarSiteStatus::Up ||
                 status == types::RadarSiteStatus::Unknown) &&
                latency.current_.has_value())
            {
               // If the radar site is up, check for high latency
               static constexpr auto kHighLatencyThreshold = 60s;

               currentLatency = GetQuantitativeTime(latency.current_.value());

               if (currentLatency > kHighLatencyThreshold)
               {
                  status = types::RadarSiteStatus::HighLatency;
               }
            }
            else if (!latency.levelTwoLastReceivedTime_.empty() &&
                     !latency.current_.has_value())
            {
               logger_->warn("No latency for {}", station.id_);
            }
         }
         else
         {
            logger_->warn("No latency information for {}", station.id_);
         }

         // Update the radar site
         auto radarSite = config::RadarSite::Get(station.id_);
         if (radarSite != nullptr)
         {
            radarSite->set_status(status);
            radarSite->set_last_received(lastReceived);
            radarSite->set_latency(currentLatency);
         }

         lastUpdate_ = now;
      }

      Q_EMIT self_->StatusUpdated();
   }
}

std::shared_ptr<RadarSiteStatusManager> RadarSiteStatusManager::Instance()
{
   static std::weak_ptr<RadarSiteStatusManager> managerReference_ {};
   static std::mutex                            instanceMutex_ {};

   const std::unique_lock lock(instanceMutex_);

   std::shared_ptr<RadarSiteStatusManager> instance = managerReference_.lock();

   if (instance == nullptr)
   {
      instance          = std::make_shared<RadarSiteStatusManager>();
      managerReference_ = instance;
   }

   return instance;
}

} // namespace scwx::qt::manager
