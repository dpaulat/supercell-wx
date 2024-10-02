#pragma once

#include <scwx/qt/types/poi_types.hpp>

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


private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace manager
} // namespace qt
} // namespace scwx
