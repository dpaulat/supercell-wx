#pragma once

#include <memory>

#include <QSortFilterProxyModel>

namespace scwx
{
namespace qt
{
namespace model
{

class AlertProxyModelImpl;

class AlertProxyModel : public QSortFilterProxyModel
{
private:
   Q_DISABLE_COPY(AlertProxyModel)

public:
   explicit AlertProxyModel(QObject* parent = nullptr);
   ~AlertProxyModel();

   void SetAlertActiveFilter(bool enabled);

   bool filterAcceptsRow(int                sourceRow,
                         const QModelIndex& sourceParent) const override;

private:
   std::unique_ptr<AlertProxyModelImpl> p;

   friend class AlertProxyModelImpl;
};

} // namespace model
} // namespace qt
} // namespace scwx
