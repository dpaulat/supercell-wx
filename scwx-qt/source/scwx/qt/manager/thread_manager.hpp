#pragma once

#include <memory>

#include <QObject>
#include <QThread>

namespace scwx
{
namespace qt
{
namespace manager
{

class ThreadManager : public QObject
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(ThreadManager)

public:
   explicit ThreadManager();
   ~ThreadManager();

   QThread* thread(const std::string& id, bool autoStart = true);

   void StopThreads();

   static ThreadManager& Instance();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace manager
} // namespace qt
} // namespace scwx
