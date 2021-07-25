#pragma once

#include <scwx/common/types.hpp>
#include <scwx/wsr88d/ar2v_file.hpp>

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

   // TODO: Improve this interface
   std::shared_ptr<const wsr88d::Ar2vFile> level2_data() const;

   void Initialize();
   void LoadLevel2Data(const std::string& filename);

private:
   std::unique_ptr<RadarManagerImpl> p;
};

} // namespace manager
} // namespace qt
} // namespace scwx
