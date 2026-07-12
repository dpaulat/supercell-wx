#pragma once

#include <scwx/common/products.hpp>
#include <scwx/provider/nexrad_data_provider.hpp>

#include <chrono>
#include <cstddef>
#include <memory>
#include <string>

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

   [[nodiscard]] std::shared_ptr<provider::NexradDataProvider> provider() const;
   void add_provider(std::shared_ptr<provider::NexradDataProvider> provider);

   [[nodiscard]] common::RadarProductGroup group() const;
   [[nodiscard]] const std::string&        product() const;

   void                      increment_refresh_count();
   void                      decrement_refresh_count();
   [[nodiscard]] std::size_t refresh_count() const;

   [[nodiscard]] bool refresh_enabled() const;
   void               set_refresh_enabled(bool enabled);

signals:
   void NewDataAvailable(common::RadarProductGroup             group,
                         const std::string&                    product,
                         std::chrono::system_clock::time_point latestTime);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::manager
