#pragma once

#include <scwx/qt/types/poi_types.hpp>

#include <string>

#include <QObject>

namespace scwx
{
namespace qt
{
namespace manager
{

class POIManager : public QObject
{
   Q_OBJECT

public:
   explicit POIManager();
   ~POIManager();

   size_t poi_count();
   types::PointOfInterest get_poi(size_t index);
   types::PointOfInterest get_poi(const std::string& name);
   void set_poi(size_t index, const types::PointOfInterest& poi);
   void set_poi(const std::string& name, const types::PointOfInterest& poi);
   void add_poi(const types::PointOfInterest& poi);
   void move_poi(size_t from, size_t to);

   static std::shared_ptr<POIManager> Instance();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace manager
} // namespace qt
} // namespace scwx
