#include <scwx/qt/model/tree_item.hpp>

namespace scwx
{
namespace qt
{
namespace model
{

static const std::string logPrefix_ = "scwx::qt::model::tree_item";

class TreeItem::Impl
{
public:
   explicit Impl(const std::vector<QVariant>& data, TreeItem* parent) :
       childItems_ {}, itemData_ {data}, parentItem_ {parent}
   {
   }
   ~Impl() { qDeleteAll(childItems_); }

   QList<TreeItem*>      childItems_;
   std::vector<QVariant> itemData_;
   TreeItem*             parentItem_;
};

TreeItem::TreeItem(const std::vector<QVariant>& data, TreeItem* parent) :
    p {std::make_unique<Impl>(data, parent)}
{
}

TreeItem::TreeItem(std::initializer_list<QVariant> data, TreeItem* parent) :
    TreeItem(std::vector<QVariant> {data}, parent)
{
}

TreeItem::~TreeItem() {}

TreeItem::TreeItem(TreeItem&&) noexcept            = default;
TreeItem& TreeItem::operator=(TreeItem&&) noexcept = default;

const TreeItem* TreeItem::child(int row) const
{
   const TreeItem* item = nullptr;

   if (0 <= row && row < p->childItems_.size())
   {
      item = p->childItems_[row];
   }

   return item;
}

TreeItem* TreeItem::child(int row)
{
   TreeItem* item = nullptr;

   if (0 <= row && row < p->childItems_.size())
   {
      item = p->childItems_[row];
   }

   return item;
}

std::vector<TreeItem*> TreeItem::children()
{
   std::vector<TreeItem*> children(p->childItems_.cbegin(),
                                   p->childItems_.cend());
   return children;
}

int TreeItem::child_count() const
{
   return static_cast<int>(p->childItems_.size());
}

int TreeItem::column_count() const
{
   return static_cast<int>(p->itemData_.size());
}

QVariant TreeItem::data(int column) const
{
   if (0 <= column && column < p->itemData_.size())
   {
      return p->itemData_[column];
   }
   else
   {
      return QVariant();
   }
}

int TreeItem::row() const
{
   int row = 0;

   if (p->parentItem_ != nullptr)
   {
      row = p->parentItem_->p->childItems_.indexOf(this);
   }

   return row;
}

const TreeItem* TreeItem::parent_item() const
{
   return p->parentItem_;
}

void TreeItem::AppendChild(TreeItem* item)
{
   item->p->parentItem_ = this;
   p->childItems_.push_back(item);
}

TreeItem* TreeItem::FindChild(int column, const QVariant& data)
{
   auto it = std::find_if(p->childItems_.begin(),
                          p->childItems_.end(),
                          [&](auto& item) {
                             return (column < item->column_count() &&
                                     item->data(column) == data);
                          });

   TreeItem* item = nullptr;
   if (it != p->childItems_.end())
   {
      item = *it;
   }

   return item;
}

bool TreeItem::InsertChildren(int position, int count, int columns)
{
   if (position < 0 || position > p->childItems_.size())
   {
      return false;
   }

   std::vector<QVariant> data(columns);

   for (int row = 0; row < count; ++row)
   {
      TreeItem* item = new TreeItem(data, this);
      p->childItems_.insert(position, item);
   }

   return true;
}

bool TreeItem::SetData(int column, const QVariant& value)
{
   if (column < 0 || column >= p->itemData_.size())
   {
      return false;
   }

   p->itemData_[column] = value;
   return true;
}

} // namespace model
} // namespace qt
} // namespace scwx
