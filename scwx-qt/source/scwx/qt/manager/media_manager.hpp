#pragma once

#include <memory>

#include <QObject>

namespace scwx
{
namespace qt
{
namespace manager
{

class MediaManager : public QObject
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(MediaManager)

public:
   explicit MediaManager();
   ~MediaManager();

   static std::shared_ptr<MediaManager> Instance();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace manager
} // namespace qt
} // namespace scwx
