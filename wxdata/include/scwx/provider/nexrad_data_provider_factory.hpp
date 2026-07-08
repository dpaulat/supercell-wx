#pragma once

#include <scwx/provider/nexrad_data_provider.hpp>

#include <memory>

namespace scwx::provider
{

class NexradDataProviderFactory
{
public:
   explicit NexradDataProviderFactory() = delete;
   ~NexradDataProviderFactory()         = delete;

   NexradDataProviderFactory(const NexradDataProviderFactory&) = delete;
   NexradDataProviderFactory&
   operator=(const NexradDataProviderFactory&) = delete;

   NexradDataProviderFactory(NexradDataProviderFactory&&) noexcept = delete;
   NexradDataProviderFactory&
   operator=(NexradDataProviderFactory&&) noexcept = delete;

   static std::shared_ptr<NexradDataProvider>
   CreateLevel2DataProvider(const std::string& radarSite);

   static std::shared_ptr<NexradDataProvider>
   CreateLevel2DataProvider(const std::string& radarSite,
                            const std::string& baseUri);

   static std::shared_ptr<NexradDataProvider>
   CreateLevel2ChunksDataProvider(const std::string& radarSite);

   static std::shared_ptr<NexradDataProvider>
   CreateLevel3DataProvider(const std::string& radarSite,
                            const std::string& product);

   static std::shared_ptr<NexradDataProvider>
   CreateLevel3DataProvider(const std::string& radarSite,
                            const std::string& product,
                            const std::string& baseUri);
};

} // namespace scwx::provider
