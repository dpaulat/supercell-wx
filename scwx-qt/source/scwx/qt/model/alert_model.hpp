#pragma once

#include <scwx/qt/types/text_event_key.hpp>

#include <memory>

#include <QAbstractTableModel>

namespace scwx
{
namespace qt
{
namespace model
{

class AlertModelImpl;

class AlertModel : public QAbstractTableModel
{
public:
   explicit AlertModel(QObject* parent = nullptr);
   ~AlertModel();

   int rowCount(const QModelIndex& parent = QModelIndex()) const override;
   int columnCount(const QModelIndex& parent = QModelIndex()) const override;

   QVariant data(const QModelIndex& index,
                 int                role = Qt::DisplayRole) const override;
   QVariant headerData(int             section,
                       Qt::Orientation orientation,
                       int             role = Qt::DisplayRole) const override;

public slots:
   void HandleAlert(const types::TextEventKey& alertKey, size_t messageIndex);
   void HandleMapUpdate(double latitude, double longitude);

private:
   std::unique_ptr<AlertModelImpl> p;

   friend class AlertModelImpl;
};

} // namespace model
} // namespace qt
} // namespace scwx
