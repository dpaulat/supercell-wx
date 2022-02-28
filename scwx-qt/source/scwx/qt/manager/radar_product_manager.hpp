#pragma once

#include <scwx/common/types.hpp>
#include <scwx/qt/config/radar_site.hpp>
#include <scwx/qt/request/nexrad_file_request.hpp>
#include <scwx/qt/types/radar_product_record.hpp>
#include <scwx/wsr88d/ar2v_file.hpp>
#include <scwx/wsr88d/level3_file.hpp>

#include <memory>
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

   const std::vector<float>& coordinates(common::RadialSize radialSize) const;
   std::shared_ptr<config::RadarSite> radar_site() const;

   // TODO: Improve this interface
   std::shared_ptr<const wsr88d::Ar2vFile> level2_data() const;

   void Initialize();
   void LoadLevel2Data(const std::string& filename);

   std::tuple<std::shared_ptr<wsr88d::rda::ElevationScan>,
              float,
              std::vector<float>>
   GetLevel2Data(wsr88d::rda::DataBlockType            dataBlockType,
                 float                                 elevation,
                 std::chrono::system_clock::time_point time = {});

   static std::shared_ptr<RadarProductManager>
   Instance(const std::string& radarSite);

   static void
   LoadData(std::istream&                               is,
            std::shared_ptr<request::NexradFileRequest> request = nullptr);
   static void
   LoadFile(const std::string&                          filename,
            std::shared_ptr<request::NexradFileRequest> request = nullptr);

signals:
   void Level2DataLoaded();

private:
   typedef std::function<std::shared_ptr<wsr88d::NexradFile>()>
      CreateNexradFileFunction;

   static void
   LoadNexradFile(CreateNexradFileFunction                    load,
                  std::shared_ptr<request::NexradFileRequest> request);

   std::unique_ptr<RadarProductManagerImpl> p;
};

} // namespace manager
} // namespace qt
} // namespace scwx
