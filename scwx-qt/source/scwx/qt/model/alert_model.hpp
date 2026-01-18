#pragma once

#include <scwx/qt/types/text_event_key.hpp>
#include <scwx/common/geographic.hpp>
#include <scwx/util/time.hpp>

#include <memory>
#include <unordered_set>

#include <boost/uuid/uuid.hpp>
#include <QAbstractTableModel>

namespace scwx::qt::model
{

class AlertModel : public QAbstractTableModel
{
   Q_DISABLE_COPY_MOVE(AlertModel)
public:
   // NOLINTNEXTLINE(performance-enum-size): Type used for Qt interface
   enum class Column : int
   {
      Etn            = 0,
      OfficeId       = 1,
      Phenomenon     = 2,
      Significance   = 3,
      ThreatCategory = 4,
      Tornado        = 5,
      State          = 6,
      Counties       = 7,
      StartTime      = 8,
      EndTime        = 9,
      Distance       = 10
   };

   explicit AlertModel(QObject* parent = nullptr);
   ~AlertModel();

   [[nodiscard]] types::TextEventKey key(const QModelIndex& index) const;
   [[nodiscard]] common::Coordinate
   centroid(const types::TextEventKey& key) const;

   [[nodiscard]] int
   rowCount(const QModelIndex& parent = QModelIndex()) const override;
   [[nodiscard]] int
   columnCount(const QModelIndex& parent = QModelIndex()) const override;

   [[nodiscard]] QVariant data(const QModelIndex& index,
                               int role = Qt::DisplayRole) const override;
   [[nodiscard]] QVariant headerData(int             section,
                                     Qt::Orientation orientation,
                                     int role = Qt::DisplayRole) const override;

public slots:
   void HandleAlert(const types::TextEventKey& alertKey,
                    std::size_t                messageIndex,
                    boost::uuids::uuid         uuid);
   void HandleAlertsRemoved(
      const std::unordered_set<types::TextEventKey,
                               types::TextEventHash<types::TextEventKey>>&
         alertKeys);
   void HandleMapUpdate(double latitude, double longitude);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::model
