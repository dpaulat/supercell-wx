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

static constexpr size_t kNumColumns = 8u;

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
      case 0:
         return QString::fromStdString(site->id());
      case 1:
         return QString::fromStdString(site->place());
      case 2:
         return QString::fromStdString(site->state());
      case 3:
         return QString::fromStdString(site->country());
      case 4:
         if (role == Qt::DisplayRole)
         {
            return QString::fromStdString(
               scwx::common::GetLatitudeString(site->latitude()));
         }
         else
         {
            return site->latitude();
         }
      case 5:
         if (role == Qt::DisplayRole)
         {
            return QString::fromStdString(
               scwx::common::GetLongitudeString(site->longitude()));
         }
         else
         {
            return site->longitude();
         }
      case 6:
         return QString::fromStdString(site->type_name());
      case 7:
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
         case 0:
            return tr("Site ID");
         case 1:
            return tr("Place");
         case 2:
            return tr("State");
         case 3:
            return tr("Country");
         case 4:
            return tr("Latitude");
         case 5:
            return tr("Longitude");
         case 6:
            return tr("Type");
         case 7:
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
