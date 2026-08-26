#pragma once

#include <scwx/qt/types/layer_types.hpp>
#include <scwx/util/iterator.hpp>

#include <istream>
#include <memory>
#include <ostream>

#include <QAbstractTableModel>

namespace scwx::qt::model
{

class LayerModel : public QAbstractTableModel
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(LayerModel)

public:
   enum class Column : int
   {
      Order       = 0,
      DisplayMap1 = 1,
      DisplayMap2 = 2,
      DisplayMap3 = 3,
      DisplayMap4 = 4,
      DisplayMap5 = 5,
      DisplayMap6 = 6,
      DisplayMap7 = 7,
      DisplayMap8 = 8,
      DisplayMap9 = 9,
      Type        = 10,
      Enabled     = 11,
      Opacity     = 12,
      Description = 13
   };
   using ColumnIterator =
      scwx::util::Iterator<Column, Column::Order, Column::Description>;

   explicit LayerModel(QObject* parent = nullptr);
   ~LayerModel();

   void ReadLayerSettings(std::istream& is);
   void WriteLayerSettings(std::ostream& os);

   [[nodiscard]] types::LayerInfo
                                    GetLayerInfo(types::LayerType        type,
                                                 types::LayerDescription description) const;
   [[nodiscard]] types::LayerVector GetLayers() const;
   void                             SetLayerDisplayed(types::LayerType        type,
                                                      types::LayerDescription description,
                                                      bool                    displayed);
   bool                             SetLayerOpacity(types::LayerType        type,
                                                    types::LayerDescription description,
                                                    float                   opacity);

   void ResetLayers();

   [[nodiscard]] int
   rowCount(const QModelIndex& parent = QModelIndex()) const override;
   [[nodiscard]] int
   columnCount(const QModelIndex& parent = QModelIndex()) const override;

   [[nodiscard]] Qt::ItemFlags   flags(const QModelIndex& index) const override;
   [[nodiscard]] Qt::DropActions supportedDropActions() const override;

   [[nodiscard]] bool IsMovable(int row) const;

   [[nodiscard]] QVariant data(const QModelIndex& index,
                               int role = Qt::DisplayRole) const override;
   [[nodiscard]] QVariant headerData(int             section,
                                     Qt::Orientation orientation,
                                     int role = Qt::DisplayRole) const override;

   bool setData(const QModelIndex& index,
                const QVariant&    value,
                int                role = Qt::EditRole) override;

   [[nodiscard]] QStringList mimeTypes() const override;
   [[nodiscard]] QMimeData*
   mimeData(const QModelIndexList& indexes) const override;

   bool dropMimeData(const QMimeData*   data,
                     Qt::DropAction     action,
                     int                row,
                     int                column,
                     const QModelIndex& parent) override;
   bool removeRows(int                row,
                   int                count,
                   const QModelIndex& parent = QModelIndex()) override;
   bool moveRows(const QModelIndex& sourceParent,
                 int                sourceRow,
                 int                count,
                 const QModelIndex& destinationParent,
                 int                destinationChild) override;

   static std::shared_ptr<LayerModel> Instance();

signals:
   void LayerDisplayChanged(types::LayerInfo layer);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::model
