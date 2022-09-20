#include <scwx/qt/model/tree_model.hpp>

namespace scwx
{
namespace qt
{
namespace model
{

static const std::string logPrefix_ = "scwx::qt::model::tree_model";

class TreeModelImpl
{
public:
   explicit TreeModelImpl() = default;
   ~TreeModelImpl()         = default;
};

TreeModel::TreeModel(QObject* parent) :
    QAbstractTableModel(parent), p(std::make_unique<TreeModelImpl>())
{
}
TreeModel::~TreeModel() = default;

int TreeModel::rowCount(const QModelIndex& parent) const
{
   const TreeItem* parentItem;

   if (parent.isValid())
   {
      parentItem = static_cast<const TreeItem*>(parent.constInternalPointer());
   }
   else
   {
      parentItem = root_item().get();
   }

   return parentItem->child_count();
}

int TreeModel::columnCount(const QModelIndex& parent) const
{
   const TreeItem* parentItem;

   if (parent.isValid())
   {
      parentItem = static_cast<const TreeItem*>(parent.constInternalPointer());
   }
   else
   {
      parentItem = root_item().get();
   }

   return parentItem->column_count();
}

QVariant TreeModel::data(const QModelIndex& index, int role) const
{
   if (!index.isValid() || role != Qt::DisplayRole)
   {
      return QVariant();
   }

   const TreeItem* item = static_cast<const TreeItem*>(index.internalPointer());

   return item->data(index.column());
}

Qt::ItemFlags TreeModel::flags(const QModelIndex& index) const
{
   Qt::ItemFlags flags;

   if (!index.isValid())
   {
      flags = Qt::NoItemFlags;
   }
   else
   {
      flags = QAbstractItemModel::flags(index);
   }

   return flags;
}

QVariant
TreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
   if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
   {
      return root_item()->data(section);
   }

   return QVariant();
}

QModelIndex
TreeModel::index(int row, int column, const QModelIndex& parent) const
{
   if (!hasIndex(row, column, parent))
   {
      return QModelIndex();
   }

   const TreeItem* parentItem;

   if (!parent.isValid())
   {
      parentItem = root_item().get();
   }
   else
   {
      parentItem = static_cast<const TreeItem*>(parent.constInternalPointer());
   }

   const TreeItem* childItem = parentItem->child(row);
   if (childItem)
   {
      return createIndex(row, column, childItem);
   }

   return QModelIndex();
}

QModelIndex TreeModel::parent(const QModelIndex& index) const
{
   if (!index.isValid())
   {
      return QModelIndex();
   }

   const TreeItem* childItem =
      static_cast<const TreeItem*>(index.constInternalPointer());
   const TreeItem* parentItem = childItem->parent_item();

   if (parentItem == root_item().get())
   {
      return QModelIndex();
   }

   return createIndex(parentItem->row(), 0, parentItem);
}

} // namespace model
} // namespace qt
} // namespace scwx
