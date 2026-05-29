#pragma once

#include <QSortFilterProxyModel>

namespace scwx
{
namespace qt
{
namespace model
{

class StormAttributeProxyModel : public QSortFilterProxyModel
{
   Q_OBJECT

public:
   explicit StormAttributeProxyModel(QObject* parent = nullptr);
   ~StormAttributeProxyModel();

protected:
   bool lessThan(const QModelIndex& left,
                 const QModelIndex& right) const override;
   bool filterAcceptsRow(int                sourceRow,
                         const QModelIndex& sourceParent) const override;

public slots:
   void SetMinDbzFilter(int minDbz);

private:
   int minDbz_ {0};
};

} // namespace model
} // namespace qt
} // namespace scwx
