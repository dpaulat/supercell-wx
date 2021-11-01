#pragma once

#include <scwx/common/color_table.hpp>
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

const std::string PRODUCT_L2_REF = "L2REF";
const std::string PRODUCT_L2_VEL = "L2VEL";
const std::string PRODUCT_L2_SW  = "L2SW";
const std::string PRODUCT_L2_ZDR = "L2ZDR";
const std::string PRODUCT_L2_PHI = "L2PHI";
const std::string PRODUCT_L2_RHO = "L2RHO";
const std::string PRODUCT_L2_CFP = "L2CFP";

class Level2ProductViewImpl;

class Level2ProductView : public RadarProductView
{
   Q_OBJECT

public:
   explicit Level2ProductView(
      const std::string&                            productName,
      std::shared_ptr<manager::RadarProductManager> radarProductManager);
   ~Level2ProductView();

   const std::vector<boost::gil::rgba8_pixel_t>& color_table() const;
   std::chrono::system_clock::time_point         sweep_time() const;
   const std::vector<float>&                     vertices() const;

   void LoadColorTable(std::shared_ptr<common::ColorTable> colorTable);

   std::tuple<const void*, size_t, size_t> GetMomentData() const;

   static std::shared_ptr<Level2ProductView>
   Create(const std::string&                            productName,
          std::shared_ptr<manager::RadarProductManager> radarProductManager);

protected slots:
   void ComputeSweep();

private:
   std::unique_ptr<Level2ProductViewImpl> p;
};

} // namespace view
} // namespace qt
} // namespace scwx
