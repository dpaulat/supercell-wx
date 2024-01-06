#pragma once

#include <scwx/qt/view/level3_product_view.hpp>

#include <chrono>
#include <memory>
#include <vector>

namespace scwx
{
namespace qt
{
namespace view
{

class Level3RasterViewImpl;

class Level3RasterView : public Level3ProductView
{
   Q_OBJECT

public:
   explicit Level3RasterView(
      const std::string&                            product,
      std::shared_ptr<manager::RadarProductManager> radarProductManager);
   ~Level3RasterView();

   float                                 range() const override;
   std::chrono::system_clock::time_point sweep_time() const override;
   std::uint16_t                         vcp() const override;
   const std::vector<float>&             vertices() const override;

   std::tuple<const void*, std::size_t, std::size_t>
   GetMomentData() const override;

   std::optional<std::uint16_t>
   GetBinLevel(const common::Coordinate& coordinate) const override;

   static std::shared_ptr<Level3RasterView>
   Create(const std::string&                            product,
          std::shared_ptr<manager::RadarProductManager> radarProductManager);

protected:
   boost::asio::thread_pool& thread_pool() override;

protected slots:
   void ComputeSweep() override;

private:
   std::unique_ptr<Level3RasterViewImpl> p;
};

} // namespace view
} // namespace qt
} // namespace scwx
