#pragma once

#include <scwx/qt/manager/radar_manager.hpp>

#include <memory>
#include <vector>

#include <QMapboxGL>

namespace scwx
{
namespace qt
{
namespace view
{

class RadarViewImpl;

class RadarView
{
public:
   explicit RadarView(std::shared_ptr<manager::RadarManager> radarManager,
                      std::shared_ptr<QMapboxGL>             map);
   ~RadarView();

   RadarView(const RadarView&) = delete;
   RadarView& operator=(const RadarView&) = delete;

   RadarView(RadarView&&) noexcept;
   RadarView& operator=(RadarView&&) noexcept;

   double                    bearing() const;
   double                    scale() const;
   const std::vector<float>& vertices() const;

   void Initialize();

private:
   std::unique_ptr<RadarViewImpl> p;
};

} // namespace view
} // namespace qt
} // namespace scwx
