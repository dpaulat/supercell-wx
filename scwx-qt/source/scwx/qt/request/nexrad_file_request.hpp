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

class NexradFileRequestImpl;

class NexradFileRequest : public QObject
{
   Q_OBJECT

public:
   explicit NexradFileRequest();
   ~NexradFileRequest();

   std::shared_ptr<types::RadarProductRecord> radar_product_record() const;

   void
   set_radar_product_record(std::shared_ptr<types::RadarProductRecord> record);

private:
   std::unique_ptr<NexradFileRequestImpl> p;

signals:
   void RequestComplete(std::shared_ptr<NexradFileRequest> request);
};

} // namespace request
} // namespace qt
} // namespace scwx
