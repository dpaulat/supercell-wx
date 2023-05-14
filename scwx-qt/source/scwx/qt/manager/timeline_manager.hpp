#pragma once

#include <memory>

#include <QObject>

namespace scwx
{
namespace qt
{
namespace manager
{

class TimelineManager : public QObject
{
   Q_OBJECT

public:
   explicit TimelineManager();
   ~TimelineManager();

   static std::shared_ptr<TimelineManager> Instance();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace manager
} // namespace qt
} // namespace scwx
