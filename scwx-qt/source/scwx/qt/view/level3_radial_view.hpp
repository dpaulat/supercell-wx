#pragma once

#include <scwx/common/color_table.hpp>
#include <scwx/common/products.hpp>
#include <scwx/qt/manager/radar_product_manager.hpp>
#include <scwx/qt/view/radar_product_view.hpp>

#include <chrono>
#include <memory>
#include <vector>

namespace scwx
{
namespace qt
{
namespace view
{

class Level3RadialViewImpl;

class Level3RadialView : public RadarProductView
{
   Q_OBJECT

public:
   explicit Level3RadialView(
      const std::string&                            product,
      std::shared_ptr<manager::RadarProductManager> radarProductManager);
   ~Level3RadialView();

   const std::vector<boost::gil::rgba8_pixel_t>& color_table() const override;
   uint16_t                              color_table_min() const override;
   uint16_t                              color_table_max() const override;
   float                                 range() const override;
   std::chrono::system_clock::time_point sweep_time() const override;
   uint16_t                              vcp() const override;
   const std::vector<float>&             vertices() const override;

   void LoadColorTable(std::shared_ptr<common::ColorTable> colorTable) override;
   void SelectElevation(float elevation) override;
   void SelectTime(std::chrono::system_clock::time_point time) override;
   void Update() override;

   common::RadarProductGroup GetRadarProductGroup() const override;
   std::string               GetRadarProductName() const override;
   std::tuple<const void*, size_t, size_t> GetMomentData() const override;

   static std::shared_ptr<Level3RadialView>
   Create(const std::string&                            product,
          std::shared_ptr<manager::RadarProductManager> radarProductManager);

protected:
   void UpdateColorTable() override;

protected slots:
   void ComputeSweep() override;

private:
   std::unique_ptr<Level3RadialViewImpl> p;
};

} // namespace view
} // namespace qt
} // namespace scwx
