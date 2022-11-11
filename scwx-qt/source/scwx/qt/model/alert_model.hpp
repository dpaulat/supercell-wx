#pragma once

#include <scwx/qt/types/text_event_key.hpp>
#include <scwx/common/geographic.hpp>
#include <scwx/util/iterator.hpp>

#include <memory>

#include <QAbstractTableModel>

namespace scwx
{
namespace qt
{
namespace model
{

class AlertModelImpl;

class AlertModel : public QAbstractTableModel
{
public:
   enum class Column : int
   {
      Etn          = 0,
      OfficeId     = 1,
      Phenomenon   = 2,
      Significance = 3,
      State        = 4,
      Counties     = 5,
      StartTime    = 6,
      EndTime      = 7,
      Distance     = 8
   };
   typedef util::Iterator<Column, Column::Etn, Column::Distance> ColumnIterator;

   explicit AlertModel(QObject* parent = nullptr);
   ~AlertModel();

   types::TextEventKey key(const QModelIndex& index) const;
   common::Coordinate  centroid(const types::TextEventKey& key) const;

   int rowCount(const QModelIndex& parent = QModelIndex()) const override;
   int columnCount(const QModelIndex& parent = QModelIndex()) const override;

   QVariant data(const QModelIndex& index,
                 int                role = Qt::DisplayRole) const override;
   QVariant headerData(int             section,
                       Qt::Orientation orientation,
                       int             role = Qt::DisplayRole) const override;

public slots:
   void HandleAlert(const types::TextEventKey& alertKey, size_t messageIndex);
   void HandleMapUpdate(double latitude, double longitude);

private:
   std::unique_ptr<AlertModelImpl> p;

   friend class AlertModelImpl;
};

} // namespace model
} // namespace qt
} // namespace scwx
