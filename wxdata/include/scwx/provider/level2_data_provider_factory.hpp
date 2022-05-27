#pragma once

#include <scwx/provider/level2_data_provider.hpp>

#include <memory>

namespace scwx
{
namespace provider
{

class Level2DataProviderFactory
{
private:
   explicit Level2DataProviderFactory() = delete;
   ~Level2DataProviderFactory()         = delete;

   Level2DataProviderFactory(const Level2DataProviderFactory&) = delete;
   Level2DataProviderFactory& operator=(const Level2DataProviderFactory&) = delete;

   Level2DataProviderFactory(Level2DataProviderFactory&&) noexcept = delete;
   Level2DataProviderFactory& operator=(Level2DataProviderFactory&&) noexcept = delete;

public:
   static std::shared_ptr<Level2DataProvider> Create(const std::string& radarSite);
};

} // namespace provider
} // namespace scwx
