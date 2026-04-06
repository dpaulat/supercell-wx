#pragma once

#include <scwx/common/color_table.hpp>
#include <scwx/common/products.hpp>
#include <scwx/qt/view/radar_product_view.hpp>

#include <chrono>
#include <memory>
#include <vector>

namespace scwx::qt::view
{

class Level2ProductView : public RadarProductView
{
   Q_OBJECT

public:
   explicit Level2ProductView(
      common::Level2Product                         product,
      std::shared_ptr<manager::RadarProductManager> radarProductManager);
   ~Level2ProductView() override;

   Level2ProductView(const Level2ProductView&)            = delete;
   Level2ProductView(Level2ProductView&&)                 = delete;
   Level2ProductView& operator=(const Level2ProductView&) = delete;
   Level2ProductView& operator=(Level2ProductView&&)      = delete;

   [[nodiscard]] std::shared_ptr<common::ColorTable>
   color_table() const override;
   [[nodiscard]] const std::vector<boost::gil::rgba8_pixel_t>&
                                      color_table_lut() const override;
   [[nodiscard]] std::uint16_t        color_table_min() const override;
   [[nodiscard]] std::uint16_t        color_table_max() const override;
   [[nodiscard]] std::optional<float> elevation() const override;
   [[nodiscard]] float                range() const override;
   [[nodiscard]] std::chrono::system_clock::time_point
                                           sweep_time() const override;
   [[nodiscard]] float                     unit_scale() const override;
   [[nodiscard]] std::string               units() const override;
   [[nodiscard]] std::uint16_t             vcp() const override;
   [[nodiscard]] const std::vector<float>& vertices() const override;

   void LoadColorTable(std::shared_ptr<common::ColorTable> colorTable) override;
   void SelectElevation(float elevation) override;
   void SelectProduct(const std::string& productName) override;

   [[nodiscard]] common::RadarProductGroup
                                    GetRadarProductGroup() const override;
   [[nodiscard]] std::string        GetRadarProductName() const override;
   [[nodiscard]] std::vector<float> GetElevationCuts() const override;
   [[nodiscard]] std::tuple<const void*, std::size_t, std::size_t>
   GetMomentData() const override;
   [[nodiscard]] std::tuple<const void*, std::size_t, std::size_t>
   GetCfpMomentData() const override;

   [[nodiscard]] std::optional<std::uint16_t>
   GetBinLevel(const common::Coordinate& coordinate) const override;
   [[nodiscard]] std::optional<wsr88d::DataLevelCode>
   GetDataLevelCode(std::uint16_t level) const override;
   [[nodiscard]] std::optional<float>
   GetDataValue(std::uint16_t level) const override;

   [[nodiscard]] std::pair<float, float> GetColorTableRange() const override;

   static std::shared_ptr<Level2ProductView>
   Create(common::Level2Product                         product,
          std::shared_ptr<manager::RadarProductManager> radarProductManager);

protected:
   boost::asio::thread_pool& thread_pool() override;

   void ConnectRadarProductManager() override;
   void DisconnectRadarProductManager() override;
   void UpdateColorTableLut() override;

protected slots:
   void ComputeSweep() override;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::view
