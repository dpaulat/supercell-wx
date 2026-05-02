#pragma once

#include <scwx/sounding/sounding_data.hpp>

#include <memory>

#include <QObject>

namespace scwx::qt::manager
{

class GfsManagerImpl;

class GfsManager : public QObject
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(GfsManager)

public:
   explicit GfsManager();
   ~GfsManager();

   static GfsManager& Instance();

public slots:
   /**
    * @brief Request a GFS sounding at a location.
    *
    * @param lat  Latitude in degrees
    * @param lon  Longitude in degrees
    * @param cycle  Model cycle (0, 6, 12, 18)
    * @param fhr    Forecast hour
    */
   void RequestSounding(double lat, double lon, int cycle, int fhr);

signals:
   /**
    * @brief Emitted when a sounding has been loaded.
    */
   void SoundingReady(std::shared_ptr<sounding::SoundingData> sounding);

   /**
    * @brief Emitted when a sounding request fails.
    */
   void LoadError(const QString& message);

private:
   std::unique_ptr<GfsManagerImpl> p;
};

} // namespace scwx::qt::manager
