#pragma once

#include <memory>

#include <QAbstractTableModel>

namespace scwx
{
namespace qt
{
namespace model
{

class RadarSiteModelImpl;

class RadarSiteModel : public QAbstractTableModel
{
public:
   explicit RadarSiteModel(QObject* parent = nullptr);
   ~RadarSiteModel();

   int rowCount(const QModelIndex& parent = QModelIndex()) const override;
   int columnCount(const QModelIndex& parent = QModelIndex()) const override;

   QVariant data(const QModelIndex& index,
                 int                role = Qt::DisplayRole) const override;
   QVariant headerData(int             section,
                       Qt::Orientation orientation,
                       int             role = Qt::DisplayRole) const override;

private:
   std::unique_ptr<RadarSiteModelImpl> p;

   friend class RadarSiteModelImpl;
};

} // namespace model
} // namespace qt
} // namespace scwx
