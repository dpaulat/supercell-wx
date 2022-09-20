#include <scwx/qt/model/radar_product_model.hpp>
#include <scwx/util/logger.hpp>

namespace scwx
{
namespace qt
{
namespace model
{

static const std::string logPrefix_ = "scwx::qt::model::radar_product_model";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class RadarProductModelItem
{
public:
   explicit RadarProductModelItem(const std::vector<QVariant>& data,
                                  RadarProductModelItem* parent = nullptr);
   ~RadarProductModelItem();

   void AppendChild(RadarProductModelItem* child);

   const RadarProductModelItem* child(int row) const;
   int                          child_count() const;
   int                          column_count() const;
   QVariant                     data(int column) const;
   int                          row() const;
   const RadarProductModelItem* parent_item() const;

private:
   std::vector<RadarProductModelItem*> childItems_;
   std::vector<QVariant>               itemData_;
   RadarProductModelItem*              parentItem_;
};

class RadarProductModelImpl
{
public:
   explicit RadarProductModelImpl() :
       rootItem_ {std::make_shared<RadarProductModelItem>(
          std::vector<QVariant> {QObject::tr("Name"), QObject::tr("Info")})}
   {
      rootItem_->AppendChild(new RadarProductModelItem(
         std::vector<QVariant> {QObject::tr("MyItem"), QObject::tr("Data")}));
   }
   ~RadarProductModelImpl() = default;

   std::shared_ptr<RadarProductModelItem> rootItem_;
};

RadarProductModel::RadarProductModel(QObject* parent) :
    QAbstractTableModel(parent), p(std::make_unique<RadarProductModelImpl>())
{
}
RadarProductModel::~RadarProductModel() = default;

int RadarProductModel::rowCount(const QModelIndex& parent) const
{
   const RadarProductModelItem* parentItem;

   if (parent.isValid())
   {
      parentItem = static_cast<const RadarProductModelItem*>(
         parent.constInternalPointer());
   }
   else
   {
      parentItem = p->rootItem_.get();
   }

   return parentItem->child_count();
}

int RadarProductModel::columnCount(const QModelIndex& parent) const
{
   const RadarProductModelItem* parentItem;

   if (parent.isValid())
   {
      parentItem = static_cast<const RadarProductModelItem*>(
         parent.constInternalPointer());
   }
   else
   {
      parentItem = p->rootItem_.get();
   }

   return parentItem->column_count();
}

QVariant RadarProductModel::data(const QModelIndex& index, int role) const
{
   if (!index.isValid() || role != Qt::DisplayRole)
   {
      return QVariant();
   }

   const RadarProductModelItem* item =
      static_cast<const RadarProductModelItem*>(index.internalPointer());

   return item->data(index.column());
}

Qt::ItemFlags RadarProductModel::flags(const QModelIndex& index) const
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

QVariant RadarProductModel::headerData(int             section,
                                       Qt::Orientation orientation,
                                       int             role) const
{
   if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
   {
      return p->rootItem_->data(section);
   }

   return QVariant();
}

QModelIndex
RadarProductModel::index(int row, int column, const QModelIndex& parent) const
{
   if (!hasIndex(row, column, parent))
   {
      return QModelIndex();
   }

   const RadarProductModelItem* parentItem;

   if (!parent.isValid())
   {
      parentItem = p->rootItem_.get();
   }
   else
   {
      parentItem = static_cast<const RadarProductModelItem*>(
         parent.constInternalPointer());
   }

   const RadarProductModelItem* childItem = parentItem->child(row);
   if (childItem)
   {
      return createIndex(row, column, childItem);
   }

   return QModelIndex();
}

QModelIndex RadarProductModel::parent(const QModelIndex& index) const
{
   if (!index.isValid())
   {
      return QModelIndex();
   }

   const RadarProductModelItem* childItem =
      static_cast<const RadarProductModelItem*>(index.constInternalPointer());
   const RadarProductModelItem* parentItem = childItem->parent_item();

   if (parentItem == p->rootItem_.get())
   {
      return QModelIndex();
   }

   return createIndex(parentItem->row(), 0, parentItem);
}

RadarProductModelItem::RadarProductModelItem(const std::vector<QVariant>& data,
                                             RadarProductModelItem* parent) :
    childItems_ {}, itemData_ {data}, parentItem_ {parent}
{
}

RadarProductModelItem::~RadarProductModelItem()
{
   qDeleteAll(childItems_);
}

void RadarProductModelItem::AppendChild(RadarProductModelItem* item)
{
   item->parentItem_ = this;
   childItems_.push_back(item);
}

const RadarProductModelItem* RadarProductModelItem::child(int row) const
{
   const RadarProductModelItem* item = nullptr;

   if (0 <= row && row < childItems_.size())
   {
      item = childItems_[row];
   }

   return item;
}

int RadarProductModelItem::child_count() const
{
   return static_cast<int>(childItems_.size());
}

int RadarProductModelItem::column_count() const
{
   return static_cast<int>(itemData_.size());
}

QVariant RadarProductModelItem::data(int column) const
{
   if (0 <= column && column < itemData_.size())
   {
      return itemData_[column];
   }
   else
   {
      return QVariant("Hello world");
   }
}

int RadarProductModelItem::row() const
{
   int row = 0;

   if (parentItem_ != nullptr)
   {
      const auto& childItems = parentItem_->childItems_;
      row =
         std::distance(childItems.cbegin(),
                       std::find(childItems.cbegin(), childItems.cend(), this));
   }

   return row;
}

const RadarProductModelItem* RadarProductModelItem::parent_item() const
{
   return parentItem_;
}

} // namespace model
} // namespace qt
} // namespace scwx
