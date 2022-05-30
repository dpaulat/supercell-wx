#pragma once

#include <scwx/provider/nexrad_data_provider.hpp>

#include <memory>

namespace scwx
{
namespace provider
{

class NexradDataProviderFactory
{
private:
   explicit NexradDataProviderFactory() = delete;
   ~NexradDataProviderFactory()         = delete;

   NexradDataProviderFactory(const NexradDataProviderFactory&) = delete;
   NexradDataProviderFactory&
   operator=(const NexradDataProviderFactory&) = delete;

   NexradDataProviderFactory(NexradDataProviderFactory&&) noexcept = delete;
   NexradDataProviderFactory&
   operator=(NexradDataProviderFactory&&) noexcept = delete;

public:
   static std::shared_ptr<NexradDataProvider>
   CreateLevel2DataProvider(const std::string& radarSite);

   static std::shared_ptr<NexradDataProvider>
   CreateLevel3DataProvider(const std::string& radarSite,
                            const std::string& product);
};

} // namespace provider
} // namespace scwx
