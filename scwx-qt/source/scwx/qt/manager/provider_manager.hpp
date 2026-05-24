#pragma once

#include <scwx/common/products.hpp>
#include <scwx/provider/nexrad_data_provider.hpp>

#include <chrono>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>

#include <boost/asio/steady_timer.hpp>
#include <boost/asio/thread_pool.hpp>
#include <QObject>

namespace scwx::qt::manager
{

class RadarProductManager;

class ProviderManager : public QObject
{
   Q_OBJECT

public:
   explicit ProviderManager(RadarProductManager*      self,
                            std::string               radarId,
                            common::RadarProductGroup group,
                            std::string               product  = "???",
                            bool                      isChunks = false);
   ~ProviderManager() override;

   ProviderManager(const ProviderManager&)            = delete;
   ProviderManager& operator=(const ProviderManager&) = delete;
   ProviderManager(ProviderManager&&)                 = delete;
   ProviderManager& operator=(ProviderManager&&)      = delete;

   [[nodiscard]] std::string name() const;

   void Disable(bool shutdown = false);
   void RefreshData();
   void RefreshDataSync();

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

signals:
   void NewDataAvailable(common::RadarProductGroup             group,
                         const std::string&                    product,
                         std::chrono::system_clock::time_point latestTime);
};

} // namespace scwx::qt::manager
