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
   explicit TreeModelImpl(TreeModel*                   self,
                          const std::vector<QVariant>& headerData) :
       self_ {self}, rootItem_ {std::make_unique<TreeItem>(headerData)} {};
   ~TreeModelImpl() = default;

   const TreeItem* item(const QModelIndex& index) const;
   TreeItem*       item(const QModelIndex& index);

   TreeModel*                self_;
   std::unique_ptr<TreeItem> rootItem_;
};

TreeModel::TreeModel(const std::vector<QVariant>& headerData, QObject* parent) :
    QAbstractItemModel(parent),
    p(std::make_unique<TreeModelImpl>(this, headerData))
{
}

TreeModel::TreeModel(std::initializer_list<QVariant> headerData,
                     QObject*                        parent) :
    TreeModel(std::vector<QVariant> {headerData}, parent)
{
}

TreeModel::~TreeModel() = default;

const TreeItem* TreeModel::root_item() const
{
   return p->rootItem_.get();
}

TreeItem* TreeModel::root_item()
{
   return p->rootItem_.get();
}

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

   if (parentItem == p->rootItem_.get() || parentItem == nullptr)
   {
      return QModelIndex();
   }

   return createIndex(parentItem->row(), 0, parentItem);
}

bool TreeModel::insertRows(int row, int count, const QModelIndex& parent)
{
   TreeItem* parentItem = p->item(parent);
   if (parentItem == nullptr)
   {
      return false;
   }

   beginInsertRows(parent, row, row + count - 1);
   bool result = parentItem->InsertChildren(row, count, columnCount(parent));
   endInsertRows();

   return result;
}

bool TreeModel::setData(const QModelIndex& index,
                        const QVariant&    value,
                        int                role)
{
   if (!index.isValid() || role != Qt::EditRole)
   {
      return false;
   }

   TreeItem* item = p->item(index);
   if (item == nullptr)
   {
      return false;
   }

   bool result = item->SetData(index.column(), value);

   if (result)
   {
      emit dataChanged(index, index);
   }

   return result;
}

void TreeModel::AppendRow(TreeItem* parent, TreeItem* child)
{
   const QModelIndex parentIndex = createIndex(parent->row(), 0, parent);
   const int         childCount  = parent->child_count();
   const int         first       = childCount;
   const int         last        = childCount;

   beginInsertRows(parentIndex, first, last);
   parent->AppendChild(child);
   endInsertRows();
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
   return rootItem_.get();
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
   return rootItem_.get();
}

} // namespace model
} // namespace qt
} // namespace scwx
