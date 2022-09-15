#pragma once

#include <memory>

#include <QAbstractTableModel>

namespace scwx
{
namespace qt
{
namespace model
{

class RadarProductModelImpl;

class RadarProductModel : QAbstractTableModel
{
public:
   explicit RadarProductModel(QObject* parent = nullptr);
   ~RadarProductModel();

private:
   std::unique_ptr<RadarProductModelImpl> p;
};

} // namespace model
} // namespace qt
} // namespace scwx
