#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <scwx/common/geographic.hpp>

#include <QAbstractTableModel>

namespace scwx
{
namespace qt
{
namespace model
{

class StormAttributeModel : public QAbstractTableModel
{
   Q_DISABLE_COPY_MOVE(StormAttributeModel)
public:
   static constexpr int SortRole = Qt::UserRole + 1;

   // NOLINTNEXTLINE(performance-enum-size): Type used for Qt interface
   enum class Column : int
   {
      StormId       = 0,
      Azimuth       = 1,
      Range         = 2,
      Direction     = 3,
      Speed         = 4,
      MaxDbz        = 5,
      MaxDbzHeight  = 6,
      ForecastError = 7,
      Distance      = 8 // Distance from map position
   };

   struct StormAttribute
   {
      std::string             stormId {};
      std::optional<uint16_t> azimuth {};       // degrees
      std::optional<uint16_t> range {};         // nmi
      std::optional<uint16_t> direction {};     // degrees
      std::optional<uint16_t> speed {};         // knots
      std::optional<int16_t>  maxDbz {};        // dBZ
      std::optional<uint32_t> maxDbzHeight {};  // feet
      std::optional<float>    forecastError {}; // nmi
      double                  distance {};      // meters
   };

   explicit StormAttributeModel(QObject* parent = nullptr);
   ~StormAttributeModel();

   [[nodiscard]] int
   rowCount(const QModelIndex& parent = QModelIndex()) const override;
   [[nodiscard]] int
   columnCount(const QModelIndex& parent = QModelIndex()) const override;

   [[nodiscard]] QVariant data(const QModelIndex& index,
                               int role = Qt::DisplayRole) const override;
   [[nodiscard]] QVariant headerData(int             section,
                                     Qt::Orientation orientation,
                                     int role = Qt::DisplayRole) const override;

   [[nodiscard]] const StormAttribute& attributeAt(int row) const;
   [[nodiscard]] common::Coordinate    stormCoordinate(int row) const;

public slots:
   void UpdateData(const std::vector<StormAttribute>& attributes,
                   const common::Coordinate&          radarSitePosition);
   void ClearData();
   void HandleMapUpdate(double latitude, double longitude);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace model
} // namespace qt
} // namespace scwx
