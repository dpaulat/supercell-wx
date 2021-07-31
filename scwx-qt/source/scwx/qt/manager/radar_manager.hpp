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

class RadarManagerImpl;

class RadarManager : public QObject
{
   Q_OBJECT

public:
   explicit RadarManager();
   ~RadarManager();

   const std::vector<float>& coordinates(common::RadialSize radialSize) const;

   // TODO: Improve this interface
   std::shared_ptr<const wsr88d::Ar2vFile> level2_data() const;

   void Initialize();
   void LoadLevel2Data(const std::string& filename);

signals:
   void Level2DataLoaded();

private:
   std::unique_ptr<RadarManagerImpl> p;
};

} // namespace manager
} // namespace qt
} // namespace scwx
