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

class LayerModel : public QAbstractTableModel
{
   Q_DISABLE_COPY_MOVE(LayerModel)

public:
   enum class Column : int
   {
      Order       = 0,
      DisplayMap1 = 1,
      DisplayMap2 = 2,
      DisplayMap3 = 3,
      DisplayMap4 = 4,
      Type        = 5,
      Enabled     = 6,
      Description = 7
   };
   typedef scwx::util::Iterator<Column, Column::Order, Column::Description>
      ColumnIterator;

   explicit LayerModel(QObject* parent = nullptr);
   ~LayerModel();

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
   void HandlePlacefileRemoved(const std::string& name);
   void HandlePlacefileRenamed(const std::string& oldName,
                               const std::string& newName);
   void HandlePlacefileUpdate(const std::string& name);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace model
} // namespace qt
} // namespace scwx
