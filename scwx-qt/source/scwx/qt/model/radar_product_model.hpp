#pragma once

#include <QAbstractItemModel>

namespace scwx
{
namespace qt
{
namespace model
{

class RadarProductModelImpl;

class RadarProductModel
{
public:
   explicit RadarProductModel();
   ~RadarProductModel();

   QAbstractItemModel* model();

private:
   std::unique_ptr<RadarProductModelImpl> p;

   friend class RadarProductModelImpl;
};

} // namespace model
} // namespace qt
} // namespace scwx
