#pragma once

#include <scwx/common/products.hpp>
#include <scwx/provider/nexrad_data_provider.hpp>
#include <scwx/wsr88d/nexrad_file.hpp>

#include <chrono>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

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
   [[nodiscard]] std::shared_ptr<provider::NexradDataProvider>
   provider(const std::string& radarId) const;
   /**
    * @brief Provider selected by first-hit refresh, or the first provider if
    * refresh has not chosen one yet.
    */
   [[nodiscard]] std::shared_ptr<provider::NexradDataProvider>
   active_provider() const;
   [[nodiscard]] std::vector<std::shared_ptr<provider::NexradDataProvider>>
        providers() const;
   void add_provider(std::shared_ptr<provider::NexradDataProvider> provider);

   /**
    * Records which provider listed the given volume times. Used so loads go to
    * the site that actually published each volume (e.g. TPBI vs TDJT). When
    * multiple providers claim the same time, the earlier entry in providers()
    * wins (canonical / post-cutover site first).
    */
   void NoteVolumeTimes(
      const std::string&                                        radarId,
      const std::vector<std::chrono::system_clock::time_point>& times);

   /**
    * Loads a volume by time from the owning provider when known, otherwise
    * from each candidate provider in order based on time. Does not fall through
    * across site aliases with a nearest-time match (once TDJT publishes, TPBI
    * is disregarded for new data).
    */
   [[nodiscard]] std::shared_ptr<wsr88d::NexradFile>
   LoadObjectByTime(std::chrono::system_clock::time_point time);

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
