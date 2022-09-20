#pragma once

#include <scwx/qt/model/tree_item.hpp>

#include <QAbstractTableModel>

namespace scwx
{
namespace qt
{
namespace model
{

class TreeModelImpl;

class TreeModel : public QAbstractTableModel
{
public:
   explicit TreeModel(QObject* parent = nullptr);
   virtual ~TreeModel();

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

protected:
   virtual const std::shared_ptr<TreeItem> root_item() const = 0;

private:
   std::unique_ptr<TreeModelImpl> p;
};

} // namespace model
} // namespace qt
} // namespace scwx
