#pragma once

#include <scwx/common/products.hpp>
#include <scwx/common/types.hpp>
#include <scwx/qt/config/radar_site.hpp>
#include <scwx/qt/request/nexrad_file_request.hpp>
#include <scwx/qt/types/radar_product_record.hpp>
#include <scwx/wsr88d/ar2v_file.hpp>
#include <scwx/wsr88d/level3_file.hpp>

#include <memory>
#include <unordered_map>
#include <vector>

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

   const std::vector<float>& coordinates(common::RadialSize radialSize) const;
   float                     gate_size() const;
   std::shared_ptr<config::RadarSite> radar_site() const;

   void Initialize();
   void EnableRefresh(common::RadarProductGroup group,
                      const std::string&        product,
                      bool                      enabled);

   std::tuple<std::shared_ptr<wsr88d::rda::ElevationScan>,
              float,
              std::vector<float>>
   GetLevel2Data(wsr88d::rda::DataBlockType            dataBlockType,
                 float                                 elevation,
                 std::chrono::system_clock::time_point time = {});

   std::shared_ptr<wsr88d::rpg::Level3Message>
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
   void                             UpdateAvailableProducts();

signals:
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
