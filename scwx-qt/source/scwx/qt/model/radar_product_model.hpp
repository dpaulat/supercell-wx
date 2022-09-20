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

   QVariant      data(const QModelIndex& index,
                      int                role = Qt::DisplayRole) const override;
   Qt::ItemFlags flags(const QModelIndex& index) const override;
   QVariant      headerData(int             section,
                            Qt::Orientation orientation,
                            int             role = Qt::DisplayRole) const override;
   QModelIndex   index(int                row,
                       int                column,
                       const QModelIndex& parent = QModelIndex()) const override;
   QModelIndex   parent(const QModelIndex& index) const override;

private:
   std::unique_ptr<RadarProductModelImpl> p;
};

} // namespace model
} // namespace qt
} // namespace scwx
