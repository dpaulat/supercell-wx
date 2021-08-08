#pragma once

#include <scwx/common/color_table.hpp>
#include <scwx/qt/manager/radar_manager.hpp>

#include <chrono>
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

class RadarView : public QObject
{
   Q_OBJECT

public:
   explicit RadarView(std::shared_ptr<manager::RadarManager> radarManager,
                      std::shared_ptr<QMapboxGL>             map);
   ~RadarView();

   double                       bearing() const;
   double                       scale() const;
   const std::vector<uint8_t>&  data_moments8() const;
   const std::vector<uint16_t>& data_moments16() const;
   const std::vector<float>&    vertices() const;

   const std::vector<boost::gil::rgba8_pixel_t>& color_table() const;

   void Initialize();
   void LoadColorTable(std::shared_ptr<common::ColorTable> colorTable);

   std::chrono::system_clock::time_point PlotTime();

public slots:
   void UpdatePlot();

signals:
   void ColorTableLoaded();
   void PlotUpdated();

private:
   std::unique_ptr<RadarViewImpl> p;
};

} // namespace view
} // namespace qt
} // namespace scwx
