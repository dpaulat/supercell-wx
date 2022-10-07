#include <scwx/qt/model/radar_site_model.hpp>
#include <scwx/qt/config/radar_site.hpp>
#include <scwx/util/logger.hpp>

namespace scwx
{
namespace qt
{
namespace model
{

static const std::string logPrefix_ = "scwx::qt::model::radar_site_model";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr size_t kNumColumns = 7u;

class RadarSiteModelImpl
{
public:
   explicit RadarSiteModelImpl();
   ~RadarSiteModelImpl() = default;

   QList<std::shared_ptr<config::RadarSite>> radarSites_;
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
       index.row() < p->radarSites_.size() && role == Qt::DisplayRole)
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
         return QString("%1").arg(site->latitude());
      case 5:
         return QString("%1").arg(site->longitude());
      case 6:
         return QString::fromStdString(site->type());
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
         default:
            break;
         }
      }
   }

   return QVariant();
}

RadarSiteModelImpl::RadarSiteModelImpl() : radarSites_ {}
{
   // Get all loaded radar sites
   std::vector<std::shared_ptr<config::RadarSite>> radarSites =
      config::RadarSite::GetAll();

   // Setup radar site list
   for (auto& site : radarSites)
   {
      radarSites_.emplace_back(std::move(site));
   }
}

} // namespace model
} // namespace qt
} // namespace scwx
