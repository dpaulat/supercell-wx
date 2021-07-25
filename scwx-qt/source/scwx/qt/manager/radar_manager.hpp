#pragma once

#include <scwx/common/types.hpp>

#include <memory>
#include <vector>

namespace scwx
{
namespace qt
{
namespace manager
{

class RadarManagerImpl;

class RadarManager
{
public:
   explicit RadarManager();
   ~RadarManager();

   RadarManager(const RadarManager&) = delete;
   RadarManager& operator=(const RadarManager&) = delete;

   RadarManager(RadarManager&&) noexcept;
   RadarManager& operator=(RadarManager&&) noexcept;

   const std::vector<float>& coordinates(common::RadialSize radialSize) const;

   void Initialize();

private:
   std::unique_ptr<RadarManagerImpl> p;
};

} // namespace manager
} // namespace qt
} // namespace scwx
