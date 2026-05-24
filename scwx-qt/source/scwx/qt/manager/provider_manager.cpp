#include <scwx/qt/manager/provider_manager.hpp>
#include <scwx/qt/manager/radar_product_manager.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/time.hpp>

#include <boost/asio/post.hpp>
#include <fmt/chrono.h>

namespace scwx::qt::manager
{

namespace
{

static const std::string logPrefix_ = "scwx::qt::manager::provider_manager";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr std::chrono::seconds kFastRetryInterval_ {15};
static constexpr std::chrono::seconds kFastRetryIntervalChunks_ {3};
static constexpr std::chrono::seconds kSlowRetryInterval_ {120};
static constexpr std::chrono::seconds kSlowRetryIntervalChunks_ {20};

} // namespace

ProviderManager::ProviderManager(RadarProductManager*      self,
                                 std::string               radarId,
                                 common::RadarProductGroup group,
                                 std::string               product,
                                 bool                      isChunks) :
    radarId_ {std::move(radarId)},
    group_ {group},
    product_ {std::move(product)},
    isChunks_ {isChunks}
{
   connect(this,
           &ProviderManager::NewDataAvailable,
           self,
           [this, self](common::RadarProductGroup             group,
                        const std::string&                    product,
                        std::chrono::system_clock::time_point latestTime)
           {
              Q_EMIT self->NewDataAvailable(
                 group, product, isChunks_, latestTime);
           });
}

ProviderManager::~ProviderManager()
{
   if (provider_ != nullptr && !providerShutdown_)
   {
      provider_->Shutdown();
   }

   providerThreadPool_.stop();
   providerThreadPool_.join();
}

std::string ProviderManager::name() const
{
   std::string name;

   if (group_ == common::RadarProductGroup::Level3)
   {
      name = fmt::format("{}, {}, {}",
                         radarId_,
                         common::GetRadarProductGroupName(group_),
                         product_);
   }
   else
   {
      name = fmt::format(
         "{}, {}", radarId_, common::GetRadarProductGroupName(group_));
   }

   return name;
}

void ProviderManager::Disable(bool shutdown)
{
   logger_->debug("Disabling refresh: {}", name());

   std::unique_lock const lock(refreshTimerMutex_);
   refreshEnabled_ = false;
   refreshTimer_.cancel();

   if (shutdown && provider_ != nullptr && !providerShutdown_)
   {
      provider_->Shutdown();
      providerShutdown_ = true;
   }
}

void ProviderManager::RefreshData()
{
   logger_->trace("RefreshData: {}", name());

   {
      const std::unique_lock lock(refreshTimerMutex_);
      refreshTimer_.cancel();
   }

   boost::asio::post(providerThreadPool_,
                     [this]()
                     {
                        try
                        {
                           RefreshDataSync();
                        }
                        catch (const std::exception& ex)
                        {
                           logger_->error(ex.what());
                        }
                     });
}

void ProviderManager::RefreshDataSync()
{
   using namespace std::chrono_literals;

   if (provider_ == nullptr)
   {
      return;
   }

   auto [newObjects, totalObjects] = provider_->Refresh();

   // Level2 chunked data is updated quickly and uses a faster interval
   const std::chrono::milliseconds fastRetryInterval =
      isChunks_ ? kFastRetryIntervalChunks_ : kFastRetryInterval_;
   const std::chrono::milliseconds slowRetryInterval =
      isChunks_ ? kSlowRetryIntervalChunks_ : kSlowRetryInterval_;
   std::chrono::milliseconds interval = fastRetryInterval;

   if (totalObjects > 0)
   {
      auto latestTime        = provider_->FindLatestTime();
      auto updatePeriod      = provider_->update_period();
      auto lastModified      = provider_->last_modified();
      auto sinceLastModified = scwx::util::time::now() - lastModified;

      // For the default interval, assume products are updated at a
      // constant rate. Expect the next product at a time based on the
      // previous two.
      interval = std::chrono::duration_cast<std::chrono::milliseconds>(
         updatePeriod - sinceLastModified);

      // Allow 5 update periods before considering the data stale
      constexpr std::size_t kUpdatePeriodStaleCount = 5;

      if (updatePeriod > 0s &&
          sinceLastModified > updatePeriod * kUpdatePeriodStaleCount)
      {
         // If it has been at least 5 update periods since the file has
         // been last modified, slow the retry period
         interval = slowRetryInterval;
      }
      else if (interval < std::chrono::milliseconds {fastRetryInterval})
      {
         // The interval should be no quicker than the fast retry interval
         interval = fastRetryInterval;
      }

      if (newObjects > 0)
      {
         Q_EMIT NewDataAvailable(group_, product_, latestTime);
      }
   }
   else if (refreshEnabled_)
   {
      logger_->info("[{}] No data found", name());

      // If no data is found, retry at the slow retry interval
      interval = slowRetryInterval;
   }

   std::unique_lock const lock(refreshTimerMutex_);

   if (refreshEnabled_)
   {
      logger_->trace(
         "[{}] Scheduled refresh in {:%M:%S}",
         name(),
         std::chrono::duration_cast<std::chrono::seconds>(interval));

      {
         refreshTimer_.expires_after(interval);
         refreshTimer_.async_wait(
            [this](const boost::system::error_code& e)
            {
               if (e == boost::system::errc::success)
               {
                  RefreshData();
               }
               else if (e == boost::asio::error::operation_aborted)
               {
                  logger_->debug("[{}] Data refresh timer cancelled", name());
               }
               else
               {
                  logger_->warn(
                     "[{}] Data refresh timer error: {}", name(), e.message());
               }
            });
      }
   }
}

} // namespace scwx::qt::manager
