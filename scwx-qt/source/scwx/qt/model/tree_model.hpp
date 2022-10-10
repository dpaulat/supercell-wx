#pragma once

#include <scwx/qt/model/tree_item.hpp>

#include <QAbstractItemModel>

namespace scwx
{
namespace qt
{
namespace model
{

class TreeModelImpl;

class TreeModel : public QAbstractItemModel
{
public:
   explicit TreeModel(const std::vector<QVariant>& headerData,
                      QObject*                     parent = nullptr);
   explicit TreeModel(std::initializer_list<QVariant> headerData,
                      QObject*                        parent = nullptr);
   virtual ~TreeModel();

   const TreeItem* root_item() const;
   TreeItem*       root_item();

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

   bool insertRows(int row, int count, const QModelIndex& parent) override;
   bool setData(const QModelIndex& index,
                const QVariant&    value,
                int                role = Qt::EditRole) override;

   void AppendRow(TreeItem* parent, TreeItem* child);

private:
   friend class TreeModelImpl;
   std::unique_ptr<TreeModelImpl> p;
};

} // namespace model
} // namespace qt
} // namespace scwx
