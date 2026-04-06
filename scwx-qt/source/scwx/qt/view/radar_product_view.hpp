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

namespace scwx::qt::view
{

class RadarProductViewImpl;

class RadarProductView : public QObject
{
   Q_OBJECT

public:
   explicit RadarProductView(
      std::shared_ptr<manager::RadarProductManager> radarProductManager);
   ~RadarProductView() override;

   RadarProductView(const RadarProductView&)            = delete;
   RadarProductView(RadarProductView&&)                 = delete;
   RadarProductView& operator=(const RadarProductView&) = delete;
   RadarProductView& operator=(RadarProductView&&)      = delete;

   [[nodiscard]] virtual std::shared_ptr<common::ColorTable>
   color_table() const = 0;
   [[nodiscard]] virtual const std::vector<boost::gil::rgba8_pixel_t>&
                                               color_table_lut() const;
   [[nodiscard]] virtual std::uint16_t         color_table_min() const;
   [[nodiscard]] virtual std::uint16_t         color_table_max() const;
   [[nodiscard]] std::optional<float>          color_table_threshold() const;
   [[nodiscard]] virtual std::optional<float>  elevation() const;
   [[nodiscard]] types::RadarProductLoadStatus load_status() const;
   [[nodiscard]] virtual float                 range() const;
   [[nodiscard]] virtual std::chrono::system_clock::time_point
                                                   sweep_time() const;
   [[nodiscard]] virtual float                     unit_scale() const = 0;
   [[nodiscard]] virtual std::string               units() const      = 0;
   [[nodiscard]] virtual std::uint16_t             vcp() const        = 0;
   [[nodiscard]] virtual const std::vector<float>& vertices() const   = 0;

   [[nodiscard]] std::shared_ptr<manager::RadarProductManager>
   radar_product_manager() const;
   [[nodiscard]] std::chrono::system_clock::time_point selected_time() const;
   [[nodiscard]] bool        show_smoothed_range_folding() const;
   [[nodiscard]] bool        smoothing_enabled() const;
   [[nodiscard]] std::mutex& sweep_mutex();

   void set_radar_product_manager(
      std::shared_ptr<manager::RadarProductManager> radarProductManager);
   void set_color_table_threshold(std::optional<float> threshold);
   void set_smoothing_enabled(bool smoothingEnabled);

   void Initialize();
   virtual void
   LoadColorTable(std::shared_ptr<common::ColorTable> colorTable) = 0;
   virtual void SelectElevation(float elevation);
   virtual void SelectProduct(const std::string& productName) = 0;
   void         SelectTime(std::chrono::system_clock::time_point time);
   void         Update();

   [[nodiscard]] bool IsInitialized() const;

   [[nodiscard]] virtual common::RadarProductGroup
                                            GetRadarProductGroup() const = 0;
   [[nodiscard]] virtual std::string        GetRadarProductName() const  = 0;
   [[nodiscard]] virtual std::vector<float> GetElevationCuts() const;
   [[nodiscard]] virtual std::tuple<const void*, std::size_t, std::size_t>
   GetMomentData() const = 0;
   [[nodiscard]] virtual std::tuple<const void*, std::size_t, std::size_t>
   GetCfpMomentData() const;

   [[nodiscard]] virtual std::optional<std::uint16_t>
   GetBinLevel(const common::Coordinate& coordinate) const = 0;
   [[nodiscard]] virtual std::optional<wsr88d::DataLevelCode>
   GetDataLevelCode(std::uint16_t level) const = 0;
   [[nodiscard]] virtual std::optional<float>
                              GetDataValue(std::uint16_t level) const = 0;
   [[nodiscard]] virtual bool IgnoreUnits() const;

   [[nodiscard]] virtual std::vector<std::pair<std::string, std::string>>
   GetDescriptionFields() const;

   [[nodiscard]] virtual std::pair<float, float> GetColorTableRange() const;

protected:
   virtual boost::asio::thread_pool& thread_pool() = 0;

   virtual void ConnectRadarProductManager()    = 0;
   virtual void DisconnectRadarProductManager() = 0;
   virtual void UpdateColorTableLut()           = 0;

   void set_load_status(types::RadarProductLoadStatus loadStatus);

protected slots:
   virtual void ComputeSweep();

signals:
   void ColorTableLutUpdated();
   void SweepComputed();
   void SweepNotComputed(types::NoUpdateReason reason);

private:
   std::unique_ptr<RadarProductViewImpl> p;
};

} // namespace scwx::qt::view
