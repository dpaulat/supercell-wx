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

class LogManager : public QObject
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(LogManager)

public:
   explicit LogManager();
   ~LogManager();

   void Initialize();
   void InitializeLogFile();

   static LogManager& Instance();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace manager
} // namespace qt
} // namespace scwx
