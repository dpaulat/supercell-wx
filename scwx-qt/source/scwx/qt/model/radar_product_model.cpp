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
   explicit RadarProductModelItem(RadarProductModelItem* parent = nullptr);
   ~RadarProductModelItem();

   void AppendChild(RadarProductModelItem* child);

   RadarProductModelItem* child(int row);
   int                    child_count() const;
   int                    column_count() const;
   QVariant               data(int column) const;
   int                    row() const;
   RadarProductModelItem* parent_item();

private:
   std::vector<RadarProductModelItem*> childItems_;
   std::vector<QVariant>               itemData_;
   RadarProductModelItem*              parentItem_;
};

class RadarProductModelImpl
{
public:
   explicit RadarProductModelImpl() {}
   ~RadarProductModelImpl() = default;
};

RadarProductModel::RadarProductModel(QObject* parent) :
    QAbstractTableModel(parent), p(std::make_unique<RadarProductModelImpl>())
{
}
RadarProductModel::~RadarProductModel() = default;

int RadarProductModel::rowCount(const QModelIndex& /*parent*/) const
{
   return 0;
}

int RadarProductModel::columnCount(const QModelIndex& /*parent*/) const
{
   return 0;
}

QVariant RadarProductModel::data(const QModelIndex& /*index*/,
                                 int /*role*/) const
{
   return QVariant();
}

RadarProductModelItem::RadarProductModelItem(RadarProductModelItem* parent) :
    childItems_ {}, itemData_ {}, parentItem_ {parent}
{
}

RadarProductModelItem::~RadarProductModelItem()
{
   qDeleteAll(childItems_);
}

void RadarProductModelItem::AppendChild(RadarProductModelItem* item)
{
   childItems_.push_back(item);
}

RadarProductModelItem* RadarProductModelItem::child(int row)
{
   RadarProductModelItem* item = nullptr;

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
      return QVariant();
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

RadarProductModelItem* RadarProductModelItem::parent_item()
{
   return parentItem_;
}

} // namespace model
} // namespace qt
} // namespace scwx
