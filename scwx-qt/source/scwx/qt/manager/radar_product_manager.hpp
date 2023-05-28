#pragma once

#include <scwx/common/products.hpp>
#include <scwx/common/types.hpp>
#include <scwx/qt/config/radar_site.hpp>
#include <scwx/qt/request/nexrad_file_request.hpp>
#include <scwx/qt/types/radar_product_record.hpp>
#include <scwx/wsr88d/ar2v_file.hpp>
#include <scwx/wsr88d/level3_file.hpp>

#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

#include <boost/uuid/nil_generator.hpp>
#include <QObject>

namespace scwx
{
namespace qt
{
namespace manager
{

class RadarProductManagerImpl;

class RadarProductManager : public QObject
{
   Q_OBJECT

public:
   explicit RadarProductManager(const std::string& radarId);
   ~RadarProductManager();

   static void Cleanup();

   /**
    * @brief Debug function to dump currently loaded products to the log.
    */
   static void DumpRecords();

   const std::vector<float>& coordinates(common::RadialSize radialSize) const;
   float                     gate_size() const;
   std::shared_ptr<config::RadarSite> radar_site() const;

   void Initialize();

   /**
    * @brief Enables or disables refresh associated with a unique identifier
    * (UUID) for a given radar product group and product.
    *
    * Only a single product refresh can be enabled for a given UUID. If a second
    * product refresh is enabled for the same UUID, the first product refresh is
    * disabled (unless still enabled under a different UUID).
    *
    * @param [in] group Radar product group
    * @param [in] product Radar product name
    * @param [in] enabled Whether to enable refresh
    * @param [in] uuid Unique identifier. Default is boost::uuids::nil_uuid().
    */
   void EnableRefresh(common::RadarProductGroup group,
                      const std::string&        product,
                      bool                      enabled,
                      boost::uuids::uuid uuid = boost::uuids::nil_uuid());

   /**
    * @brief Gets a merged list of the volume times for products with refresh
    * enabled. The volume times will be for the previous, current and next day.
    *
    * @param [in] time Current date to provide to volume time query
    *
    * @return Merged list of active volume times
    */
   std::set<std::chrono::system_clock::time_point>
   GetActiveVolumeTimes(std::chrono::system_clock::time_point time);

   /**
    * @brief Get level 2 radar data for a data block type, elevation, and time.
    *
    * @param [in] dataBlockType Data block type
    * @param [in] elevation Elevation tilt
    * @param [in] time Radar product time
    *
    * @return Level 2 radar data, selected elevation cut, available elevation
    * cuts and selected time
    */
   std::tuple<std::shared_ptr<wsr88d::rda::ElevationScan>,
              float,
              std::vector<float>,
              std::chrono::system_clock::time_point>
   GetLevel2Data(wsr88d::rda::DataBlockType            dataBlockType,
                 float                                 elevation,
                 std::chrono::system_clock::time_point time = {});

   /**
    * @brief Get level 3 message data for a product and time.
    *
    * @param [in] product Radar product name
    * @param [in] time Radar product time
    *
    * @return Level 3 message data and selected time
    */
   std::tuple<std::shared_ptr<wsr88d::rpg::Level3Message>,
              std::chrono::system_clock::time_point>
   GetLevel3Data(const std::string&                    product,
                 std::chrono::system_clock::time_point time = {});

   static std::shared_ptr<RadarProductManager>
   Instance(const std::string& radarSite);

   void LoadLevel2Data(
      std::chrono::system_clock::time_point       time,
      std::shared_ptr<request::NexradFileRequest> request = nullptr);
   void LoadLevel3Data(
      const std::string&                          product,
      std::chrono::system_clock::time_point       time,
      std::shared_ptr<request::NexradFileRequest> request = nullptr);

   static void
   LoadData(std::istream&                               is,
            std::shared_ptr<request::NexradFileRequest> request = nullptr);
   static void
   LoadFile(const std::string&                          filename,
            std::shared_ptr<request::NexradFileRequest> request = nullptr);

   common::Level3ProductCategoryMap GetAvailableLevel3Categories();
   std::vector<std::string>         GetLevel3Products();

   /**
    * @brief Set the maximum number of products of each type that may be cached.
    *
    * @param [in] cacheLimit The maximum number of products of each type
    */
   void SetCacheLimit(std::size_t cacheLimit);

   void UpdateAvailableProducts();

signals:
   void DataReloaded(std::shared_ptr<types::RadarProductRecord> record);
   void Level3ProductsChanged();
   void NewDataAvailable(common::RadarProductGroup             group,
                         const std::string&                    product,
                         std::chrono::system_clock::time_point latestTime);

private:
   std::unique_ptr<RadarProductManagerImpl> p;

   friend class RadarProductManagerImpl;
};

} // namespace manager
} // namespace qt
} // namespace scwx
