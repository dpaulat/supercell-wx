#pragma once

#include <scwx/wsr88d/nexrad_file.hpp>

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

   std::shared_ptr<wsr88d::NexradFile> nexrad_file() const;

   void set_nexrad_file(std::shared_ptr<wsr88d::NexradFile> nexradFile);

private:
   std::unique_ptr<NexradFileRequestImpl> p;

signals:
   void RequestComplete(std::shared_ptr<NexradFileRequest> request);
};

} // namespace request
} // namespace qt
} // namespace scwx
