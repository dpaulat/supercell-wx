#include "gps_info_dialog.hpp"
#include "ui_gps_info_dialog.h"

#include <scwx/qt/manager/position_manager.hpp>
#include <scwx/common/geographic.hpp>
#include <scwx/util/time.hpp>

#include <units/angle.h>
#include <units/length.h>
#include <units/velocity.h>
#include <QClipboard>
#include <QGeoPositionInfo>

namespace scwx
{
namespace qt
{
namespace ui
{

static const QString kDisabledString_ = "---";

class GpsInfoDialog::Impl
{
public:
   explicit Impl(GpsInfoDialog* self) : self_ {self} {};
   ~Impl() = default;

   std::shared_ptr<manager::PositionManager> positionManager_ {
      manager::PositionManager::Instance()};

   void Update(const QGeoPositionInfo& info, bool updateTime = true);

   GpsInfoDialog* self_;
};

GpsInfoDialog::GpsInfoDialog(QWidget* parent) :
    QDialog(parent), p {std::make_unique<Impl>(this)}, ui(new Ui::GpsInfoDialog)
{
   ui->setupUi(this);

   p->Update({}, false);

   connect(p->positionManager_.get(),
           &manager::PositionManager::PositionUpdated,
           this,
           [this](const QGeoPositionInfo& info) { p->Update(info); });

   connect(ui->copyCoordinateButton,
           &QAbstractButton::clicked,
           this,
           [this]()
           {
              QClipboard* clipboard = QGuiApplication::clipboard();
              clipboard->setText(ui->coordinateLabel->text());
           });
}

GpsInfoDialog::~GpsInfoDialog()
{
   delete ui;
}

void GpsInfoDialog::Impl::Update(const QGeoPositionInfo& info, bool updateTime)
{
   auto coordinate = info.coordinate();

   if (coordinate.isValid())
   {
      const QString latitude = QString::fromStdString(
         common::GetLatitudeString(coordinate.latitude()));
      const QString longitude = QString::fromStdString(
         common::GetLongitudeString(coordinate.longitude()));

      self_->ui->coordinateLabel->setText(
         QString("%1, %2").arg(latitude).arg(longitude));
   }
   else
   {
      self_->ui->coordinateLabel->setText(kDisabledString_);
   }

   if (coordinate.type() == QGeoCoordinate::CoordinateType::Coordinate3D)
   {
      units::length::meters<double> altitude {coordinate.altitude()};
      self_->ui->altitudeLabel->setText(
         QString::fromStdString(units::to_string(altitude)));
   }
   else
   {
      self_->ui->altitudeLabel->setText(kDisabledString_);
   }

   if (info.hasAttribute(QGeoPositionInfo::Attribute::Direction))
   {
      units::angle::degrees<double> direction {
         info.attribute(QGeoPositionInfo::Attribute::Direction)};
      self_->ui->directionLabel->setText(
         QString::fromStdString(units::to_string(direction)));
   }
   else
   {
      self_->ui->directionLabel->setText(kDisabledString_);
   }

   if (info.hasAttribute(QGeoPositionInfo::Attribute::GroundSpeed))
   {
      units::velocity::meters_per_second<double> groundSpeed {
         info.attribute(QGeoPositionInfo::Attribute::GroundSpeed)};
      self_->ui->groundSpeedLabel->setText(
         QString::fromStdString(units::to_string(groundSpeed)));
   }
   else
   {
      self_->ui->groundSpeedLabel->setText(kDisabledString_);
   }

   if (info.hasAttribute(QGeoPositionInfo::Attribute::VerticalSpeed))
   {
      units::velocity::meters_per_second<double> verticalSpeed {
         info.attribute(QGeoPositionInfo::Attribute::VerticalSpeed)};
      self_->ui->verticalSpeedLabel->setText(
         QString::fromStdString(units::to_string(verticalSpeed)));
   }
   else
   {
      self_->ui->verticalSpeedLabel->setText(kDisabledString_);
   }

   if (info.hasAttribute(QGeoPositionInfo::Attribute::MagneticVariation))
   {
      units::angle::degrees<double> magneticVariation {
         info.attribute(QGeoPositionInfo::Attribute::MagneticVariation)};
      self_->ui->magneticVariationLabel->setText(
         QString::fromStdString(units::to_string(magneticVariation)));
   }
   else
   {
      self_->ui->magneticVariationLabel->setText(kDisabledString_);
   }

   if (info.hasAttribute(QGeoPositionInfo::Attribute::HorizontalAccuracy))
   {
      units::length::meters<double> horizontalAccuracy {
         info.attribute(QGeoPositionInfo::Attribute::HorizontalAccuracy)};
      if (!std::isnan(horizontalAccuracy.value()))
      {
         self_->ui->horizontalAccuracyLabel->setText(
            QString::fromStdString(units::to_string(horizontalAccuracy)));
      }
      else
      {
         self_->ui->horizontalAccuracyLabel->setText(kDisabledString_);
      }
   }
   else
   {
      self_->ui->horizontalAccuracyLabel->setText(kDisabledString_);
   }

   if (info.hasAttribute(QGeoPositionInfo::Attribute::VerticalAccuracy))
   {
      units::length::meters<double> verticalAccuracy {
         info.attribute(QGeoPositionInfo::Attribute::VerticalAccuracy)};
      if (!std::isnan(verticalAccuracy.value()))
      {
         self_->ui->verticalAccuracyLabel->setText(
            QString::fromStdString(units::to_string(verticalAccuracy)));
      }
      else
      {
         self_->ui->verticalAccuracyLabel->setText(kDisabledString_);
      }
   }
   else
   {
      self_->ui->verticalAccuracyLabel->setText(kDisabledString_);
   }

   if (info.hasAttribute(QGeoPositionInfo::Attribute::DirectionAccuracy))
   {
      units::angle::degrees<double> directionAccuracy {
         info.attribute(QGeoPositionInfo::Attribute::DirectionAccuracy)};
      self_->ui->directionAccuracyLabel->setText(
         QString::fromStdString(units::to_string(directionAccuracy)));
   }
   else
   {
      self_->ui->directionAccuracyLabel->setText(kDisabledString_);
   }

   if (updateTime)
   {
      self_->ui->lastUpdateLabel->setText(
         info.timestamp().toString(Qt::DateFormat::ISODate));
   }
}

} // namespace ui
} // namespace qt
} // namespace scwx
