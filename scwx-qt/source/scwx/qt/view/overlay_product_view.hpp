#pragma once

#include <scwx/qt/types/radar_product_record.hpp>

#include <chrono>
#include <memory>

#include <QObject>

namespace scwx
{
namespace qt
{
namespace manager
{

class RadarProductManager;

} // namespace manager

namespace view
{

class OverlayProductView : public QObject
{
   Q_OBJECT

public:
   explicit OverlayProductView();
   virtual ~OverlayProductView();

   std::shared_ptr<manager::RadarProductManager> radar_product_manager() const;
   std::shared_ptr<types::RadarProductRecord>
   radar_product_record(const std::string& product) const;
   std::chrono::system_clock::time_point selected_time() const;

   void set_radar_product_manager(
      const std::shared_ptr<manager::RadarProductManager>& radarProductManager);

   void SelectTime(std::chrono::system_clock::time_point time);
   void SetAutoRefresh(bool enabled);
   void SetAutoUpdate(bool enabled);

signals:
   void ProductUpdated(std::string product);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace view
} // namespace qt
} // namespace scwx
