#pragma once

#include <scwx/common/color_table.hpp>
#include <scwx/common/geographic.hpp>
#include <scwx/common/products.hpp>
#include <scwx/qt/manager/radar_product_manager.hpp>
#include <scwx/qt/types/map_types.hpp>
#include <scwx/wsr88d/wsr88d_types.hpp>

#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include <QObject>
#include <boost/asio/thread_pool.hpp>

namespace scwx
{
namespace qt
{
namespace view
{

class RadarProductViewImpl;

class RadarProductView : public QObject
{
   Q_OBJECT

public:
   explicit RadarProductView(
      std::shared_ptr<manager::RadarProductManager> radarProductManager);
   virtual ~RadarProductView();

   virtual std::shared_ptr<common::ColorTable> color_table() const = 0;
   virtual const std::vector<boost::gil::rgba8_pixel_t>&
                                                 color_table_lut() const;
   virtual std::uint16_t                         color_table_min() const;
   virtual std::uint16_t                         color_table_max() const;
   virtual float                                 elevation() const;
   virtual float                                 range() const;
   virtual std::chrono::system_clock::time_point sweep_time() const;
   virtual std::uint16_t                         vcp() const      = 0;
   virtual const std::vector<float>&             vertices() const = 0;

   std::shared_ptr<manager::RadarProductManager> radar_product_manager() const;
   std::chrono::system_clock::time_point         selected_time() const;
   std::mutex&                                   sweep_mutex();

   void set_radar_product_manager(
      std::shared_ptr<manager::RadarProductManager> radarProductManager);

   void Initialize();
   virtual void
   LoadColorTable(std::shared_ptr<common::ColorTable> colorTable) = 0;
   virtual void SelectElevation(float elevation);
   virtual void SelectProduct(const std::string& productName) = 0;
   void         SelectTime(std::chrono::system_clock::time_point time);
   void         Update();

   bool IsInitialized() const;

   virtual common::RadarProductGroup GetRadarProductGroup() const = 0;
   virtual std::string               GetRadarProductName() const  = 0;
   virtual std::vector<float>        GetElevationCuts() const;
   virtual std::tuple<const void*, std::size_t, std::size_t>
   GetMomentData() const = 0;
   virtual std::tuple<const void*, std::size_t, std::size_t>
   GetCfpMomentData() const;

   virtual std::optional<std::uint16_t>
   GetBinLevel(const common::Coordinate& coordinate) const = 0;
   virtual std::optional<wsr88d::DataLevelCode>
                                GetDataLevelCode(std::uint16_t level) const = 0;
   virtual std::optional<float> GetDataValue(std::uint16_t level) const     = 0;

   std::chrono::system_clock::time_point GetSelectedTime() const;

   virtual std::vector<std::pair<std::string, std::string>>
   GetDescriptionFields() const;

protected:
   virtual boost::asio::thread_pool& thread_pool() = 0;

   virtual void ConnectRadarProductManager()    = 0;
   virtual void DisconnectRadarProductManager() = 0;
   virtual void UpdateColorTableLut()           = 0;

protected slots:
   virtual void ComputeSweep();

signals:
   void ColorTableLutUpdated();
   void SweepComputed();
   void SweepNotComputed(types::NoUpdateReason reason);

private:
   std::unique_ptr<RadarProductViewImpl> p;
};

} // namespace view
} // namespace qt
} // namespace scwx
