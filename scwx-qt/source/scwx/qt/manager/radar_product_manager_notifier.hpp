#pragma once

#include <memory>

#include <QObject>

namespace scwx
{
namespace qt
{
namespace manager
{

class RadarProductManagerNotifierImpl;

class RadarProductManagerNotifier : public QObject
{
   Q_OBJECT

public:
   explicit RadarProductManagerNotifier();
   ~RadarProductManagerNotifier();

   static RadarProductManagerNotifier& Instance();

signals:
   void RadarProductManagerCreated(const std::string& radarSite);

private:
   std::unique_ptr<RadarProductManagerNotifierImpl> p;
};

} // namespace manager
} // namespace qt
} // namespace scwx
