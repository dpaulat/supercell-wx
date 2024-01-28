#pragma once

#include <scwx/qt/types/radar_product_record.hpp>

#include <memory>

#include <QObject>

namespace scwx
{
namespace qt
{
namespace request
{

class NexradFileRequest : public QObject
{
   Q_OBJECT

public:
   explicit NexradFileRequest(const std::string& currentRadarSite = {});
   ~NexradFileRequest();

   std::string                                current_radar_site() const;
   std::shared_ptr<types::RadarProductRecord> radar_product_record() const;

   void set_radar_product_record(
      const std::shared_ptr<types::RadarProductRecord>& record);

private:
   class Impl;
   std::unique_ptr<Impl> p;

signals:
   void RequestComplete(std::shared_ptr<NexradFileRequest> request);
};

} // namespace request
} // namespace qt
} // namespace scwx
