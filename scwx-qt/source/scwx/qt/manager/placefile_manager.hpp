#pragma once

#include <scwx/gr/placefile.hpp>

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

   bool PlacefileEnabled(const std::string& name);
   bool PlacefileThresholded(const std::string& name);
   std::shared_ptr<const gr::Placefile> Placefile(const std::string& name);

   /**
    * @brief Gets a list of active placefiles
    *
    * @return Vector of placefile pointers
    */
   std::vector<std::shared_ptr<gr::Placefile>> GetActivePlacefiles();

   void AddUrl(const std::string& urlString);
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
