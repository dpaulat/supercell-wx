#include <scwx/qt/manager/provider_manager.hpp>
#include <scwx/qt/manager/radar_product_manager.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/time.hpp>

#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/thread_pool.hpp>
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

class ProviderManager::Impl
{
public:
   Impl(std::string               radarId,
        common::RadarProductGroup group,
        std::string               product,
        bool                      isChunks) :
       radarId_ {std::move(radarId)},
       group_ {group},
       product_ {std::move(product)},
       isChunks_ {isChunks}
   {
   }

   boost::asio::thread_pool providerThreadPool_ {2u};

   const std::string               radarId_;
   const common::RadarProductGroup group_;
   const std::string               product_;
   const bool                      isChunks_;
   bool                            refreshEnabled_ {false};
   boost::asio::steady_timer       refreshTimer_ {providerThreadPool_};
   std::mutex                      refreshTimerMutex_ {};
   std::shared_ptr<provider::NexradDataProvider> provider_ {nullptr};
   std::size_t                                   refreshCount_ {0};
   bool                                          providerShutdown_ {false};
};

ProviderManager::ProviderManager(RadarProductManager*      self,
                                 std::string               radarId,
                                 common::RadarProductGroup group,
                                 std::string               product,
                                 bool                      isChunks) :
    p(std::make_unique<Impl>(
       std::move(radarId), group, std::move(product), isChunks))
{
   connect(this,
           &ProviderManager::NewDataAvailable,
           self,
           [this, self](common::RadarProductGroup             group,
                        const std::string&                    product,
                        std::chrono::system_clock::time_point latestTime)
           {
              Q_EMIT self->NewDataAvailable(
                 group, product, p->isChunks_, latestTime);
           });
}

ProviderManager::~ProviderManager()
{
   if (p->provider_ != nullptr && !p->providerShutdown_)
   {
      p->provider_->Shutdown();
   }

   p->providerThreadPool_.stop();
   p->providerThreadPool_.join();
}

std::string ProviderManager::name() const
{
   std::string name;

   if (p->group_ == common::RadarProductGroup::Level3)
   {
      name = fmt::format("{}, {}, {}",
                         p->radarId_,
                         common::GetRadarProductGroupName(p->group_),
                         p->product_);
   }
   else
   {
      name = fmt::format(
         "{}, {}", p->radarId_, common::GetRadarProductGroupName(p->group_));
   }

   return name;
}

void ProviderManager::Disable(bool shutdown)
{
   logger_->debug("Disabling refresh: {}", name());

   std::unique_lock const lock(p->refreshTimerMutex_);
   p->refreshEnabled_ = false;
   p->refreshTimer_.cancel();

   if (shutdown && p->provider_ != nullptr && !p->providerShutdown_)
   {
      p->provider_->Shutdown();
      p->providerShutdown_ = true;
   }
}

void ProviderManager::RefreshData()
{
   logger_->trace("RefreshData: {}", name());

   {
      const std::unique_lock lock(p->refreshTimerMutex_);
      p->refreshTimer_.cancel();
   }

   boost::asio::post(p->providerThreadPool_,
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

   if (p->provider_ == nullptr)
   {
      return;
   }

   auto [newObjects, totalObjects] = p->provider_->Refresh();

   // Level2 chunked data is updated quickly and uses a faster interval
   const std::chrono::milliseconds fastRetryInterval =
      p->isChunks_ ? kFastRetryIntervalChunks_ : kFastRetryInterval_;
   const std::chrono::milliseconds slowRetryInterval =
      p->isChunks_ ? kSlowRetryIntervalChunks_ : kSlowRetryInterval_;
   std::chrono::milliseconds interval = fastRetryInterval;

   if (totalObjects > 0)
   {
      auto latestTime        = p->provider_->FindLatestTime();
      auto updatePeriod      = p->provider_->update_period();
      auto lastModified      = p->provider_->last_modified();
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
         Q_EMIT NewDataAvailable(p->group_, p->product_, latestTime);
      }
   }
   else if (p->refreshEnabled_)
   {
      logger_->info("[{}] No data found", name());

      // If no data is found, retry at the slow retry interval
      interval = slowRetryInterval;
   }

   std::unique_lock const lock(p->refreshTimerMutex_);

   if (p->refreshEnabled_)
   {
      logger_->trace(
         "[{}] Scheduled refresh in {:%M:%S}",
         name(),
         std::chrono::duration_cast<std::chrono::seconds>(interval));

      {
         p->refreshTimer_.expires_after(interval);
         p->refreshTimer_.async_wait(
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

std::shared_ptr<provider::NexradDataProvider> ProviderManager::provider() const
{
   return p->provider_;
}

void ProviderManager::set_provider(
   std::shared_ptr<provider::NexradDataProvider> provider)
{
   p->provider_ = std::move(provider);
}

common::RadarProductGroup ProviderManager::group() const
{
   return p->group_;
}

const std::string& ProviderManager::product() const
{
   return p->product_;
}

void ProviderManager::increment_refresh_count()
{
   ++p->refreshCount_;
}

void ProviderManager::decrement_refresh_count()
{
   --p->refreshCount_;
}

std::size_t ProviderManager::refresh_count() const
{
   return p->refreshCount_;
}

bool ProviderManager::refresh_enabled() const
{
   return p->refreshEnabled_;
}

void ProviderManager::set_refresh_enabled(bool enabled)
{
   p->refreshEnabled_ = enabled;
}

} // namespace scwx::qt::manager
