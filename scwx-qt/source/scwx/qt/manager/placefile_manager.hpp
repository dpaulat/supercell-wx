#pragma once

#include <memory>
#include <string>

#include <QObject>

namespace scwx
{
namespace qt
{
namespace manager
{

class PlacefileManager : public QObject
{
   Q_OBJECT

public:
   explicit PlacefileManager();
   ~PlacefileManager();

   void LoadFile(const std::string& filename);

   static std::shared_ptr<PlacefileManager> Instance();

signals:
   void PlacefileEnabled(const std::string& name, bool enabled);
   void PlacefileUpdated(const std::string& name);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace manager
} // namespace qt
} // namespace scwx
