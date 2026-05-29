#include <scwx/qt/model/storm_attribute_model.hpp>
#include <scwx/qt/util/geographic_lib.hpp>
#include <scwx/qt/settings/unit_settings.hpp>
#include <scwx/qt/types/unit_types.hpp>
#include <scwx/util/logger.hpp>

#include <fmt/format.h>

#include <GeographicLib/Geodesic.hpp>

namespace scwx::qt::model
{

static constexpr int kFirstColumn = 0;
static constexpr int kLastColumn =
   static_cast<int>(StormAttributeModel::Column::Distance);
static constexpr int kNumColumns = kLastColumn + 1;

class StormAttributeModel::Impl
{
public:
   explicit Impl() :
       geodesic_ {scwx::qt::util::GeographicLib::DefaultGeodesic()}
   {
   }
   ~Impl() = default;

   std::vector<StormAttributeModel::StormAttribute> stormAttributes_ {};
   common::Coordinate                               radarSitePosition_ {};
   std::vector<common::Coordinate>                  stormCoordinates_ {};
   common::Coordinate                               currentPosition_ {};

   const GeographicLib::Geodesic& geodesic_;
};

StormAttributeModel::StormAttributeModel(QObject* parent) :
    QAbstractTableModel(parent), p(std::make_unique<Impl>())
{
}

StormAttributeModel::~StormAttributeModel() = default;

int StormAttributeModel::rowCount(const QModelIndex& parent) const
{
   Q_UNUSED(parent);
   return static_cast<int>(p->stormAttributes_.size());
}

int StormAttributeModel::columnCount(const QModelIndex& parent) const
{
   Q_UNUSED(parent);
   return kNumColumns;
}

QVariant StormAttributeModel::data(const QModelIndex& index, int role) const
{
   if (!index.isValid() ||
       index.row() >= static_cast<int>(p->stormAttributes_.size()))
   {
      return QVariant();
   }

   const auto& attr =
      p->stormAttributes_[static_cast<std::size_t>(index.row())];
   auto column = static_cast<Column>(index.column());

   if (role == Qt::DisplayRole || role == Qt::ToolTipRole)
   {
      switch (column)
      {
      case Column::StormId:
         return QString::fromStdString(attr.stormId);

      case Column::Azimuth:
         if (attr.azimuth.has_value())
         {
            return QString::fromStdString(
               fmt::format("{}\u00B0", attr.azimuth.value()));
         }
         break;

      case Column::Range:
         if (attr.range.has_value())
         {
            return QString::fromStdString(
               fmt::format("{} nmi", attr.range.value()));
         }
         break;

      case Column::Direction:
         if (attr.direction.has_value())
         {
            return QString::fromStdString(
               fmt::format("{}\u00B0", attr.direction.value()));
         }
         break;

      case Column::Speed:
         if (attr.speed.has_value())
         {
            return QString::fromStdString(
               fmt::format("{} kt", attr.speed.value()));
         }
         break;

      case Column::MaxDbz:
         if (attr.maxDbz.has_value())
         {
            return QString::fromStdString(
               fmt::format("{} dBZ", attr.maxDbz.value()));
         }
         break;

      case Column::MaxDbzHeight:
         if (attr.maxDbzHeight.has_value())
         {
            return QString::fromStdString(
               fmt::format("{} kft", attr.maxDbzHeight.value() / 1000u));
         }
         break;

      case Column::ForecastError:
         if (attr.forecastError.has_value())
         {
            return QString::fromStdString(
               fmt::format("{:.1f} nmi", attr.forecastError.value()));
         }
         break;

      case Column::Distance:
         if (role == Qt::DisplayRole)
         {
            const std::string distanceUnitName =
               settings::UnitSettings::Instance().distance_units().GetValue();
            types::DistanceUnits distanceUnits =
               types::GetDistanceUnitsFromName(distanceUnitName);
            double distanceScale = types::GetDistanceUnitsScale(distanceUnits);
            std::string abbreviation =
               types::GetDistanceUnitsAbbreviation(distanceUnits);

            double distanceKm = attr.distance * common::kKilometersPerMeter;
            return QString("%1 %2")
               .arg(static_cast<uint32_t>(distanceKm * distanceScale))
               .arg(QString::fromStdString(abbreviation));
         }
         else
         {
            return attr.distance;
         }
      }
   }
   else if (role == Qt::TextAlignmentRole)
   {
      if (column == Column::StormId)
      {
         return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
      }
      return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
   }
   else if (role == SortRole)
   {
      switch (column)
      {
      case Column::StormId:
         return QString::fromStdString(attr.stormId);
      case Column::Azimuth:
         return static_cast<int>(attr.azimuth.value_or(0));
      case Column::Range:
         return static_cast<int>(attr.range.value_or(0));
      case Column::Direction:
         return static_cast<int>(attr.direction.value_or(0));
      case Column::Speed:
         return static_cast<int>(attr.speed.value_or(0));
      case Column::MaxDbz:
         return static_cast<int>(attr.maxDbz.value_or(0));
      case Column::MaxDbzHeight:
         return static_cast<qint64>(attr.maxDbzHeight.value_or(0));
      case Column::ForecastError:
         return static_cast<double>(attr.forecastError.value_or(0.0f));
      case Column::Distance:
         return attr.distance;
      }
   }

   return QVariant();
}

QVariant StormAttributeModel::headerData(int             section,
                                         Qt::Orientation orientation,
                                         int             role) const
{
   if (role != Qt::DisplayRole)
   {
      return QVariant();
   }

   if (orientation == Qt::Horizontal)
   {
      switch (static_cast<Column>(section))
      {
      case Column::StormId:
         return tr("ID");
      case Column::Azimuth:
         return tr("Az");
      case Column::Range:
         return tr("Range");
      case Column::Direction:
         return tr("Dir");
      case Column::Speed:
         return tr("Speed");
      case Column::MaxDbz:
         return tr("Max dBZ");
      case Column::MaxDbzHeight:
         return tr("Height");
      case Column::ForecastError:
         return tr("Fcst Err");
      case Column::Distance:
         return tr("Distance");
      }
   }

   return QVariant();
}

const StormAttributeModel::StormAttribute&
StormAttributeModel::attributeAt(int row) const
{
   return p->stormAttributes_.at(static_cast<std::size_t>(row));
}

void StormAttributeModel::UpdateData(
   const std::vector<StormAttribute>& attributes,
   const common::Coordinate&          radarSitePosition)
{
   beginResetModel();
   p->stormAttributes_   = attributes;
   p->radarSitePosition_ = radarSitePosition;

   // Pre-compute storm coordinates and distances
   p->stormCoordinates_.resize(attributes.size());
   for (size_t i = 0; i < attributes.size(); ++i)
   {
      if (attributes[i].azimuth.has_value() && attributes[i].range.has_value())
      {
         p->stormCoordinates_[i] = common::polar_to_latlon(
            radarSitePosition.latitude_,
            radarSitePosition.longitude_,
            0.0,
            static_cast<double>(attributes[i].azimuth.value()),
            static_cast<double>(attributes[i].range.value()));

         // Calculate distance from current map position
         double distanceMeters = 0.0;
         if (p->currentPosition_ != common::Coordinate {0.0, 0.0})
         {
            p->geodesic_.Inverse(p->currentPosition_.latitude_,
                                 p->currentPosition_.longitude_,
                                 p->stormCoordinates_[i].latitude_,
                                 p->stormCoordinates_[i].longitude_,
                                 distanceMeters);
         }
         p->stormAttributes_[i].distance = distanceMeters;
      }
      else
      {
         p->stormCoordinates_[i]         = {0.0, 0.0};
         p->stormAttributes_[i].distance = 0.0;
      }
   }
   endResetModel();
}

void StormAttributeModel::ClearData()
{
   beginResetModel();
   p->stormAttributes_.clear();
   p->stormCoordinates_.clear();
   endResetModel();
}

void StormAttributeModel::HandleMapUpdate(double latitude, double longitude)
{
   p->currentPosition_ = {latitude, longitude};

   // Recalculate distances for all storms
   for (size_t i = 0; i < p->stormAttributes_.size(); ++i)
   {
      auto& attr = p->stormAttributes_[i];

      if (p->stormCoordinates_[i] != common::Coordinate {0.0, 0.0})
      {
         double distanceMeters = 0.0;
         p->geodesic_.Inverse(latitude,
                              longitude,
                              p->stormCoordinates_[i].latitude_,
                              p->stormCoordinates_[i].longitude_,
                              distanceMeters);
         attr.distance = distanceMeters;
      }
      else
      {
         attr.distance = 0.0;
      }
   }

   // Emit dataChanged for the Distance column
   const int rows = rowCount();
   if (rows > 0)
   {
      const auto topLeft = createIndex(0, static_cast<int>(Column::Distance));
      const auto bottomRight =
         createIndex(rows - 1, static_cast<int>(Column::Distance));
      Q_EMIT dataChanged(topLeft, bottomRight);
   }
}

common::Coordinate StormAttributeModel::stormCoordinate(int row) const
{
   return p->stormCoordinates_.at(static_cast<std::size_t>(row));
}

} // namespace scwx::qt::model
