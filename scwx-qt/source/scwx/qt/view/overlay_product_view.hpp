#pragma once

#include <chrono>
#include <memory>

#include <QObject>

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

class Level3Message;

} // namespace rpg
} // namespace wsr88d

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
   std::shared_ptr<wsr88d::rpg::Level3Message>
   radar_product_message(const std::string& product) const;
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
