#include <scwx/qt/manager/provider_manager.hpp>
#include <scwx/qt/manager/radar_product_manager.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/time.hpp>

#include <limits>
#include <map>
#include <shared_mutex>

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

   [[nodiscard]] std::size_t ProviderIndex(const std::string& radarId) const;

   boost::asio::thread_pool providerThreadPool_ {2u};

   const std::string               radarId_;
   const common::RadarProductGroup group_;
   const std::string               product_;
   const bool                      isChunks_;
   bool                            refreshEnabled_ {false};
   boost::asio::steady_timer       refreshTimer_ {providerThreadPool_};
   std::mutex                      refreshTimerMutex_ {};
   std::size_t                     refreshCount_ {0};
   bool                            providersShutdown_ {false};
   std::atomic<bool>               firstRefreshComplete_ {false};

   std::vector<std::shared_ptr<provider::NexradDataProvider>> providers_ {};
   // Sticky site chosen by first-hit refresh (TDJT once it publishes; else TPBI
   // during the cutover uncertainty window).
   mutable std::shared_mutex                     lastProviderMutex_ {};
   std::shared_ptr<provider::NexradDataProvider> lastProvider_ {};

   mutable std::shared_mutex volumeTimeOwnersMutex_ {};
   std::map<std::chrono::system_clock::time_point, std::string>
      volumeTimeOwners_ {};
};

std::size_t
ProviderManager::Impl::ProviderIndex(const std::string& radarId) const
{
   for (std::size_t i = 0; i < providers_.size(); ++i)
   {
      if (providers_[i]->radar_site() == radarId)
      {
         return i;
      }
   }

   return std::numeric_limits<std::size_t>::max();
}

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
   if (!p->providersShutdown_)
   {
      for (const auto& provider : p->providers_)
      {
         provider->Shutdown();
      }
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

   if (shutdown && !p->providersShutdown_)
   {
      for (const auto& provider : p->providers_)
      {
         provider->Shutdown();
      }
      p->providersShutdown_ = true;
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

   if (p->providers_.empty())
   {
      return;
   }

   std::size_t newObjects   = 0;
   std::size_t totalObjects = 0;

   std::chrono::system_clock::time_point latestTime {};

   // Level2 chunked data is updated quickly and uses a faster interval
   const std::chrono::milliseconds fastRetryInterval =
      p->isChunks_ ? kFastRetryIntervalChunks_ : kFastRetryInterval_;
   const std::chrono::milliseconds slowRetryInterval =
      p->isChunks_ ? kSlowRetryIntervalChunks_ : kSlowRetryInterval_;
   std::chrono::milliseconds interval = fastRetryInterval;

   // First-hit-wins across site aliases (e.g. TDJT then TPBI). The transition
   // window is cutover uncertainty: once the canonical site publishes, stick
   // to it and disregard the legacy site.
   for (const auto& provider : p->providers_)
   {
      auto [providerNewObjects, providerTotalObjects] = provider->Refresh();

      // Only update the latest time and interval if this is the first provider
      // to have data
      if (providerTotalObjects > 0 && totalObjects == 0)
      {
         latestTime             = provider->FindLatestTime();
         auto updatePeriod      = provider->update_period();
         auto lastModified      = provider->last_modified();
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

         {
            const std::unique_lock lock(p->lastProviderMutex_);
            p->lastProvider_ = provider;
         }
      }

      newObjects += providerNewObjects;
      totalObjects += providerTotalObjects;

      // Stop after the active provider has data, or remains the sticky
      // provider. On the first pass, continue so an empty TDJT can fall back
      // to TPBI before cutover.
      const std::shared_lock lock(p->lastProviderMutex_);
      if (p->firstRefreshComplete_ &&
          (providerTotalObjects > 0 || p->lastProvider_ == provider))
      {
         break;
      }
   }

   if (newObjects > 0)
   {
      p->firstRefreshComplete_ = true;
      Q_EMIT NewDataAvailable(p->group_, p->product_, latestTime);
   }
   else if (totalObjects == 0 && p->refreshEnabled_)
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
   return (p->providers_.empty() ? nullptr : p->providers_.front());
}

std::shared_ptr<provider::NexradDataProvider>
ProviderManager::provider(const std::string& radarId) const
{
   // If there is only one provider, return it
   if (p->providers_.size() == 1)
   {
      return p->providers_.front();
   }

   // If there are multiple providers, find the one with the matching radar site
   for (const auto& provider : p->providers_)
   {
      if (provider->radar_site() == radarId)
      {
         return provider;
      }
   }

   logger_->warn("No provider found for radar ID: {}", radarId);

   return nullptr;
}

std::shared_ptr<provider::NexradDataProvider>
ProviderManager::active_provider() const
{
   const std::shared_lock lock(p->lastProviderMutex_);

   if (p->lastProvider_ != nullptr)
   {
      return p->lastProvider_;
   }

   return provider();
}

std::vector<std::shared_ptr<provider::NexradDataProvider>>
ProviderManager::providers() const
{
   return p->providers_;
}

void ProviderManager::add_provider(
   std::shared_ptr<provider::NexradDataProvider> provider)
{
   p->providers_.emplace_back(std::move(provider));
}

void ProviderManager::NoteVolumeTimes(
   const std::string&                                        radarId,
   const std::vector<std::chrono::system_clock::time_point>& times)
{
   if (times.empty())
   {
      return;
   }

   const std::unique_lock lock {p->volumeTimeOwnersMutex_};
   const std::size_t      newIndex = p->ProviderIndex(radarId);

   for (const auto& time : times)
   {
      auto it = p->volumeTimeOwners_.find(time);
      if (it == p->volumeTimeOwners_.end())
      {
         p->volumeTimeOwners_.emplace(time, radarId);
      }
      else if (newIndex < p->ProviderIndex(it->second))
      {
         it->second = radarId;
      }
   }
}

std::shared_ptr<wsr88d::NexradFile>
ProviderManager::LoadObjectByTime(std::chrono::system_clock::time_point time)
{
   // Prefer the provider that listed this volume time.
   {
      const std::shared_lock lock {p->volumeTimeOwnersMutex_};
      const auto             it = p->volumeTimeOwners_.find(time);
      if (it != p->volumeTimeOwners_.cend())
      {
         const auto ownedProvider = provider(it->second);
         if (ownedProvider != nullptr)
         {
            if (auto nexradFile = ownedProvider->LoadObjectByTime(time);
                nexradFile != nullptr)
            {
               return nexradFile;
            }
         }
      }
   }

   // Fall back across providers in candidate order
   for (const auto& candidateProvider : p->providers_)
   {
      if (auto nexradFile = candidateProvider->LoadObjectByTime(time);
          nexradFile != nullptr)
      {
         return nexradFile;
      }
   }

   return nullptr;
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
