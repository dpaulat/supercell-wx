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
   explicit TreeModelImpl(TreeModel* self) : self_ {self} {};
   ~TreeModelImpl() = default;

   const TreeItem* item(const QModelIndex& index) const;
   TreeItem*       item(const QModelIndex& index);

   TreeModel* self_;
};

TreeModel::TreeModel(QObject* parent) :
    QAbstractItemModel(parent), p(std::make_unique<TreeModelImpl>(this))
{
}
TreeModel::~TreeModel() = default;

int TreeModel::rowCount(const QModelIndex& parent) const
{
   const TreeItem* parentItem = p->item(parent);
   return parentItem ? parentItem->child_count() : 0;
}

int TreeModel::columnCount(const QModelIndex& /* parent */) const
{
   return root_item()->column_count();
}

QVariant TreeModel::data(const QModelIndex& index, int role) const
{
   if (!index.isValid() || role != Qt::DisplayRole)
   {
      return QVariant();
   }

   const TreeItem* item = p->item(index);

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
   if (parent.isValid() && parent.column() != 0)
   {
      return QModelIndex();
   }

   const TreeItem* parentItem = p->item(parent);
   if (parentItem == nullptr)
   {
      return QModelIndex();
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

   const TreeItem* childItem  = p->item(index);
   const TreeItem* parentItem = childItem ? childItem->parent_item() : nullptr;

   if (parentItem == root_item().get() || parentItem == nullptr)
   {
      return QModelIndex();
   }

   return createIndex(parentItem->row(), 0, parentItem);
}

const TreeItem* TreeModelImpl::item(const QModelIndex& index) const
{
   if (index.isValid())
   {
      const TreeItem* item =
         static_cast<const TreeItem*>(index.constInternalPointer());
      if (item != nullptr)
      {
         return item;
      }
   }
   return self_->root_item().get();
}

TreeItem* TreeModelImpl::item(const QModelIndex& index)
{
   if (index.isValid())
   {
      TreeItem* item = static_cast<TreeItem*>(index.internalPointer());
      if (item != nullptr)
      {
         return item;
      }
   }
   return self_->root_item().get();
}

} // namespace model
} // namespace qt
} // namespace scwx
