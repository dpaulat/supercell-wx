#pragma once

#include <scwx/common/types.hpp>
#include <scwx/wsr88d/ar2v_file.hpp>

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
   explicit RadarProductManager();
   ~RadarProductManager();

   const std::vector<float>& coordinates(common::RadialSize radialSize) const;

   // TODO: Improve this interface
   std::shared_ptr<const wsr88d::Ar2vFile> level2_data() const;

   void Initialize();
   void LoadLevel2Data(const std::string& filename);

   std::shared_ptr<wsr88d::rda::ElevationScan>
   GetLevel2Data(wsr88d::rda::DataBlockType            dataBlockType,
                 uint16_t                              elevation,
                 std::chrono::system_clock::time_point time = {});

signals:
   void Level2DataLoaded();

private:
   std::unique_ptr<RadarProductManagerImpl> p;
};

} // namespace manager
} // namespace qt
} // namespace scwx
