#include <scwx/qt/model/storm_attribute_proxy_model.hpp>
#include <scwx/qt/model/storm_attribute_model.hpp>

namespace scwx::qt::model
{

StormAttributeProxyModel::StormAttributeProxyModel(QObject* parent) :
    QSortFilterProxyModel(parent)
{
   setSortRole(StormAttributeModel::SortRole);
}

StormAttributeProxyModel::~StormAttributeProxyModel() = default;

bool StormAttributeProxyModel::lessThan(const QModelIndex& left,
                                        const QModelIndex& right) const
{
   QVariant leftData = sourceModel()->data(left, StormAttributeModel::SortRole);
   QVariant rightData =
      sourceModel()->data(right, StormAttributeModel::SortRole);

   if (leftData.typeId() == QMetaType::Int)
   {
      return leftData.toInt() < rightData.toInt();
   }
   if (leftData.typeId() == QMetaType::LongLong)
   {
      return leftData.toLongLong() < rightData.toLongLong();
   }
   if (leftData.typeId() == QMetaType::Double)
   {
      return leftData.toDouble() < rightData.toDouble();
   }
   if (leftData.typeId() == QMetaType::QString)
   {
      return leftData.toString() < rightData.toString();
   }

   return QSortFilterProxyModel::lessThan(left, right);
}

bool StormAttributeProxyModel::filterAcceptsRow(
   int sourceRow, const QModelIndex& sourceParent) const
{
   // Check min dBZ filter
   if (minDbz_ > 0)
   {
      QModelIndex dbzIndex = sourceModel()->index(
         sourceRow,
         static_cast<int>(StormAttributeModel::Column::MaxDbz),
         sourceParent);
      QVariant dbzData =
         sourceModel()->data(dbzIndex, StormAttributeModel::SortRole);

      if (dbzData.isValid() && dbzData.toInt() < minDbz_)
      {
         return false;
      }
   }

   // Check text filter (delegates to base class)
   return QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
}

void StormAttributeProxyModel::SetMinDbzFilter(int minDbz)
{
   minDbz_ = minDbz;
   beginFilterChange();
   endFilterChange();
}

} // namespace scwx::qt::model
