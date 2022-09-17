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

class RadarProductModel : public QAbstractTableModel
{
public:
   explicit RadarProductModel(QObject* parent = nullptr);
   ~RadarProductModel();

   int rowCount(const QModelIndex& parent = QModelIndex()) const override;
   int columnCount(const QModelIndex& parent = QModelIndex()) const override;
   QVariant data(const QModelIndex& index,
                 int                role = Qt::DisplayRole) const override;

private:
   std::unique_ptr<RadarProductModelImpl> p;
};

} // namespace model
} // namespace qt
} // namespace scwx
