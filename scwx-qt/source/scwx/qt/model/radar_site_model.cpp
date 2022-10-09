#include <scwx/qt/model/radar_site_model.hpp>
#include <scwx/qt/common/types.hpp>
#include <scwx/qt/config/radar_site.hpp>
#include <scwx/common/geographic.hpp>
#include <scwx/util/logger.hpp>

#include <format>

#include <GeographicLib/Geodesic.hpp>

namespace scwx
{
namespace qt
{
namespace model
{

static const std::string logPrefix_ = "scwx::qt::model::radar_site_model";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr size_t kColumnSiteId    = 0u;
static constexpr size_t kColumnPlace     = 1u;
static constexpr size_t kColumnState     = 2u;
static constexpr size_t kColumnCountry   = 3u;
static constexpr size_t kColumnLatitude  = 4u;
static constexpr size_t kColumnLongitude = 5u;
static constexpr size_t kColumnType      = 6u;
static constexpr size_t kColumnDistance  = 7u;
static constexpr size_t kNumColumns      = 8u;

class RadarSiteModelImpl
{
public:
   explicit RadarSiteModelImpl();
   ~RadarSiteModelImpl() = default;

   QList<std::shared_ptr<config::RadarSite>> radarSites_;

   GeographicLib::Geodesic geodesic_;

   std::unordered_map<std::string, double> distanceMap_;
   scwx::common::DistanceType              distanceDisplay_;
   scwx::common::Coordinate                previousPosition_;
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
   return parent.isValid() ? 0 : static_cast<int>(kNumColumns);
}

QVariant RadarSiteModel::data(const QModelIndex& index, int role) const
{
   if (index.isValid() && index.row() >= 0 &&
       index.row() < p->radarSites_.size() &&
       (role == Qt::DisplayRole || role == common::SortRole))
   {
      const auto& site = p->radarSites_.at(index.row());

      switch (index.column())
      {
      case kColumnSiteId:
         return QString::fromStdString(site->id());
      case kColumnPlace:
         return QString::fromStdString(site->place());
      case kColumnState:
         return QString::fromStdString(site->state());
      case kColumnCountry:
         return QString::fromStdString(site->country());
      case kColumnLatitude:
         if (role == Qt::DisplayRole)
         {
            return QString::fromStdString(
               scwx::common::GetLatitudeString(site->latitude()));
         }
         else
         {
            return site->latitude();
         }
      case kColumnLongitude:
         if (role == Qt::DisplayRole)
         {
            return QString::fromStdString(
               scwx::common::GetLongitudeString(site->longitude()));
         }
         else
         {
            return site->longitude();
         }
      case kColumnType:
         return QString::fromStdString(site->type_name());
      case kColumnDistance:
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
         case kColumnSiteId:
            return tr("Site ID");
         case kColumnPlace:
            return tr("Place");
         case kColumnState:
            return tr("State");
         case kColumnCountry:
            return tr("Country");
         case kColumnLatitude:
            return tr("Latitude");
         case kColumnLongitude:
            return tr("Longitude");
         case kColumnType:
            return tr("Type");
         case kColumnDistance:
            return tr("Distance");
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

   QModelIndex topLeft     = createIndex(0, kColumnDistance);
   QModelIndex bottomRight = createIndex(rowCount() - 1, kColumnDistance);

   emit dataChanged(topLeft, bottomRight);
}

RadarSiteModelImpl::RadarSiteModelImpl() :
    radarSites_ {},
    geodesic_(GeographicLib::Constants::WGS84_a(),
              GeographicLib::Constants::WGS84_f()),
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
   }
}

} // namespace model
} // namespace qt
} // namespace scwx
