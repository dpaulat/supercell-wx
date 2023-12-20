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
   Q_OBJECT

public:
   enum class Column : int
   {
      SiteId    = 0,
      Place     = 1,
      State     = 2,
      Country   = 3,
      Latitude  = 4,
      Longitude = 5,
      Type      = 6,
      Distance  = 7,
      Favorite  = 8
   };

   explicit RadarSiteModel(QObject* parent = nullptr);
   ~RadarSiteModel();

   int rowCount(const QModelIndex& parent = QModelIndex()) const override;
   int columnCount(const QModelIndex& parent = QModelIndex()) const override;

   QVariant data(const QModelIndex& index,
                 int                role = Qt::DisplayRole) const override;
   QVariant headerData(int             section,
                       Qt::Orientation orientation,
                       int             role = Qt::DisplayRole) const override;

   void HandleMapUpdate(double latitude, double longitude);
   void ToggleFavorite(int row);

   static std::shared_ptr<RadarSiteModel> Instance();

signals:
   void FavoriteToggled(const std::string& siteId, bool isFavorite);

private:
   std::unique_ptr<RadarSiteModelImpl> p;

   friend class RadarSiteModelImpl;
};

} // namespace model
} // namespace qt
} // namespace scwx
