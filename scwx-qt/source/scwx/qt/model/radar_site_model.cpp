#include <scwx/qt/model/radar_site_model.hpp>
#include <scwx/qt/config/radar_site.hpp>
#include <scwx/qt/types/qt_types.hpp>
#include <scwx/qt/util/geographic_lib.hpp>
#include <scwx/common/geographic.hpp>
#include <scwx/util/logger.hpp>

#include <QIcon>

namespace scwx
{
namespace qt
{
namespace model
{

static const std::string logPrefix_ = "scwx::qt::model::radar_site_model";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr int kFirstColumn =
   static_cast<int>(RadarSiteModel::Column::SiteId);
static constexpr int kLastColumn =
   static_cast<int>(RadarSiteModel::Column::Favorite);
static constexpr int kNumColumns = kLastColumn - kFirstColumn + 1;

class RadarSiteModelImpl
{
public:
   explicit RadarSiteModelImpl();
   ~RadarSiteModelImpl() = default;

   QList<std::shared_ptr<config::RadarSite>> radarSites_;
   std::vector<bool>                         favorites_;

   const GeographicLib::Geodesic& geodesic_;

   std::unordered_map<std::string, double> distanceMap_;
   scwx::common::DistanceType              distanceDisplay_;
   scwx::common::Coordinate                previousPosition_;

   QIcon starIcon_ {":/res/icons/font-awesome-6/star-solid.svg"};
};

RadarSiteModel::RadarSiteModel(QObject* parent) :
    QAbstractTableModel(parent), p(std::make_unique<RadarSiteModelImpl>())
{
}
RadarSiteModel::~RadarSiteModel() = default;

int RadarSiteModel::rowCount(const QModelIndex& parent) const
{
   return parent.isValid() ? 0 : p->radarSites_.size();
}

int RadarSiteModel::columnCount(const QModelIndex& parent) const
{
   return parent.isValid() ? 0 : kNumColumns;
}

QVariant RadarSiteModel::data(const QModelIndex& index, int role) const
{
   if (!index.isValid() || index.row() < 0 ||
       index.row() >= p->radarSites_.size())
   {
      return QVariant();
   }

   const auto& site = p->radarSites_.at(index.row());

   if (role == Qt::DisplayRole || role == types::SortRole)
   {
      switch (index.column())
      {
      case static_cast<int>(Column::SiteId):
         return QString::fromStdString(site->id());
      case static_cast<int>(Column::Place):
         return QString::fromStdString(site->place());
      case static_cast<int>(Column::State):
         return QString::fromStdString(site->state());
      case static_cast<int>(Column::Country):
         return QString::fromStdString(site->country());
      case static_cast<int>(Column::Latitude):
         if (role == Qt::DisplayRole)
         {
            return QString::fromStdString(
               scwx::common::GetLatitudeString(site->latitude()));
         }
         else
         {
            return site->latitude();
         }
      case static_cast<int>(Column::Longitude):
         if (role == Qt::DisplayRole)
         {
            return QString::fromStdString(
               scwx::common::GetLongitudeString(site->longitude()));
         }
         else
         {
            return site->longitude();
         }
      case static_cast<int>(Column::Type):
         return QString::fromStdString(site->type_name());
      case static_cast<int>(Column::Distance):
         if (role == Qt::DisplayRole)
         {
            if (p->distanceDisplay_ == scwx::common::DistanceType::Miles)
            {
               return QString("%1 mi").arg(
                  static_cast<uint32_t>(p->distanceMap_.at(site->id()) *
                                        scwx::common::kMilesPerMeter));
            }
            else
            {
               return QString("%1 km").arg(
                  static_cast<uint32_t>(p->distanceMap_.at(site->id()) *
                                        scwx::common::kKilometersPerMeter));
            }
         }
         else
         {
            return p->distanceMap_.at(site->id());
         }
      case static_cast<int>(Column::Favorite):
         if (role == types::SortRole)
         {
            return QVariant(p->favorites_.at(index.row()));
         }
         break;
      default:
         break;
      }
   }
   else if (role == Qt::DecorationRole)
   {
      switch (index.column())
      {
      case static_cast<int>(Column::Favorite):
         if (p->favorites_.at(index.row()))
         {
            return p->starIcon_;
         }
         break;
      default:
         break;
      }
   }

   return QVariant();
}

QVariant RadarSiteModel::headerData(int             section,
                                    Qt::Orientation orientation,
                                    int             role) const
{
   if (role == Qt::DisplayRole)
   {
      if (orientation == Qt::Horizontal)
      {
         switch (section)
         {
         case static_cast<int>(Column::SiteId):
            return tr("Site ID");
         case static_cast<int>(Column::Place):
            return tr("Place");
         case static_cast<int>(Column::State):
            return tr("State");
         case static_cast<int>(Column::Country):
            return tr("Country");
         case static_cast<int>(Column::Latitude):
            return tr("Latitude");
         case static_cast<int>(Column::Longitude):
            return tr("Longitude");
         case static_cast<int>(Column::Type):
            return tr("Type");
         case static_cast<int>(Column::Distance):
            return tr("Distance");
         default:
            break;
         }
      }
   }
   else if (role == Qt::DecorationRole)
   {
      if (orientation == Qt::Horizontal)
      {
         switch (section)
         {
         case static_cast<int>(Column::Favorite):
            return p->starIcon_;
         default:
            break;
         }
      }
   }

   return QVariant();
}

void RadarSiteModel::HandleMapUpdate(double latitude, double longitude)
{
   logger_->trace("Handle map update: {}, {}", latitude, longitude);

   double distanceInMeters;

   for (const auto& site : p->radarSites_)
   {
      p->geodesic_.Inverse(latitude,
                           longitude,
                           site->latitude(),
                           site->longitude(),
                           distanceInMeters);
      p->distanceMap_[site->id()] = distanceInMeters;
   }

   QModelIndex topLeft = createIndex(0, static_cast<int>(Column::Distance));
   QModelIndex bottomRight =
      createIndex(rowCount() - 1, static_cast<int>(Column::Distance));

   Q_EMIT dataChanged(topLeft, bottomRight);
}

void RadarSiteModel::ToggleFavorite(int row)
{
   if (row >= 0 && row < p->favorites_.size())
   {
      bool isFavorite       = !p->favorites_.at(row);
      p->favorites_.at(row) = isFavorite;

      QModelIndex index = createIndex(row, static_cast<int>(Column::Favorite));
      Q_EMIT dataChanged(index, index);

      Q_EMIT FavoriteToggled(p->radarSites_.at(row)->id(), isFavorite);
   }
}

RadarSiteModelImpl::RadarSiteModelImpl() :
    radarSites_ {},
    geodesic_(util::GeographicLib::DefaultGeodesic()),
    distanceMap_ {},
    distanceDisplay_ {scwx::common::DistanceType::Miles},
    previousPosition_ {}
{
   // Get all loaded radar sites
   std::vector<std::shared_ptr<config::RadarSite>> radarSites =
      config::RadarSite::GetAll();

   // Setup radar site list
   for (auto& site : radarSites)
   {
      distanceMap_[site->id()] = 0.0;
      radarSites_.emplace_back(std::move(site));
      favorites_.emplace_back(false);
   }
}

std::shared_ptr<RadarSiteModel> RadarSiteModel::Instance()
{
   static std::weak_ptr<RadarSiteModel> radarSiteModelReference_ {};
   static std::mutex                    instanceMutex_ {};

   std::unique_lock lock(instanceMutex_);

   std::shared_ptr<RadarSiteModel> radarSiteModel =
      radarSiteModelReference_.lock();

   if (radarSiteModel == nullptr)
   {
      radarSiteModel           = std::make_shared<RadarSiteModel>();
      radarSiteModelReference_ = radarSiteModel;
   }

   return radarSiteModel;
}

} // namespace model
} // namespace qt
} // namespace scwx
