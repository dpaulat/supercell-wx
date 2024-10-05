#pragma once

#include <QAbstractTableModel>

namespace scwx
{
namespace qt
{
namespace model
{

class MarkerModel : public QAbstractTableModel
{
public:
   enum class Column : int
   {
      Name      = 0,
      Latitude  = 1,
      Longitude = 2
   };

   explicit MarkerModel(QObject* parent = nullptr);
   ~MarkerModel();

   int rowCount(const QModelIndex& parent = QModelIndex()) const override;
   int columnCount(const QModelIndex& parent = QModelIndex()) const override;

   Qt::ItemFlags flags(const QModelIndex& index) const override;

   QVariant data(const QModelIndex& index,
                 int                role = Qt::DisplayRole) const override;
   QVariant headerData(int             section,
                       Qt::Orientation orientation,
                       int             role = Qt::DisplayRole) const override;

   bool setData(const QModelIndex& index,
                const QVariant&    value,
                int                role = Qt::EditRole) override;


public slots:
   void HandleMarkerAdded();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace model
} // namespace qt
} // namespace scwx
