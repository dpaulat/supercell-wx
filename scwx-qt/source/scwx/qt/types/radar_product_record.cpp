#include <scwx/qt/types/radar_product_record.hpp>
#include <scwx/qt/config/radar_site.hpp>
#include <scwx/common/sites.hpp>
#include <scwx/util/time.hpp>

namespace scwx
{
namespace qt
{
namespace types
{

static const std::string logPrefix_ = "scwx::qt::types::radar_product_record";

class RadarProductRecordImpl
{
public:
   explicit RadarProductRecordImpl(
      std::shared_ptr<wsr88d::NexradFile> nexradFile) :
       nexradFile_ {nexradFile},
       radarId_ {"????"},
       radarProduct_ {"???"},
       radarProductGroup_ {common::RadarProductGroup::Unknown},
       siteId_ {"???"},
       time_ {util::TimePoint(0, 0)}
   {
   }

   ~RadarProductRecordImpl() {}

   std::shared_ptr<wsr88d::NexradFile>   nexradFile_;
   std::string                           radarId_;
   std::string                           radarProduct_;
   common::RadarProductGroup             radarProductGroup_;
   std::string                           siteId_;
   std::chrono::system_clock::time_point time_;
};

RadarProductRecord::RadarProductRecord(
   std::shared_ptr<wsr88d::NexradFile> nexradFile) :
    p(std::make_unique<RadarProductRecordImpl>(nexradFile))
{
   std::shared_ptr<wsr88d::Ar2vFile>   level2File = level2_file();
   std::shared_ptr<wsr88d::Level3File> level3File = level3_file();

   uint16_t julianDate   = 0;
   uint32_t milliseconds = 0;

   if (level2File != nullptr)
   {
      p->radarProductGroup_ = common::RadarProductGroup::Level2;
      p->radarId_           = level2File->icao();
      p->siteId_            = common::GetSiteId(p->radarId_);
      julianDate            = level2File->julian_date();
      milliseconds          = level2File->milliseconds();
   }
   else if (level3File != nullptr)
   {
      p->radarProductGroup_ = common::RadarProductGroup::Level3;
      p->radarProduct_      = level3File->wmo_header()->product_category();
      p->siteId_            = level3File->wmo_header()->product_designator();
      p->radarId_           = config::GetRadarIdFromSiteId(p->siteId_);

      auto descriptionBlock = level3File->message()->description_block();

      if (descriptionBlock != nullptr)
      {
         julianDate   = descriptionBlock->volume_scan_date();
         milliseconds = descriptionBlock->volume_scan_start_time() * 1000u;
      }
      else
      {
         julianDate = level3File->message()->header().date_of_message();
         milliseconds =
            level3File->message()->header().time_of_message() * 1000u;
      }
   }

   p->time_ = util::TimePoint(julianDate, milliseconds);
}

RadarProductRecord::~RadarProductRecord() = default;

RadarProductRecord::RadarProductRecord(RadarProductRecord&&) noexcept = default;
RadarProductRecord&
RadarProductRecord::operator=(RadarProductRecord&&) noexcept = default;

std::shared_ptr<wsr88d::Ar2vFile> RadarProductRecord::level2_file() const
{
   return std::dynamic_pointer_cast<wsr88d::Ar2vFile>(p->nexradFile_);
}

std::shared_ptr<wsr88d::Level3File> RadarProductRecord::level3_file() const
{
   return std::dynamic_pointer_cast<wsr88d::Level3File>(p->nexradFile_);
}

std::shared_ptr<wsr88d::NexradFile> RadarProductRecord::nexrad_file() const
{
   return p->nexradFile_;
}

std::string RadarProductRecord::radar_id() const
{
   return p->radarId_;
}

std::string RadarProductRecord::radar_product() const
{
   return p->radarProduct_;
}

common::RadarProductGroup RadarProductRecord::radar_product_group() const
{
   return p->radarProductGroup_;
}

std::string RadarProductRecord::site_id() const
{
   return p->siteId_;
}

std::chrono::system_clock::time_point RadarProductRecord::time() const
{
   return p->time_;
}

std::shared_ptr<RadarProductRecord>
RadarProductRecord::Create(std::shared_ptr<wsr88d::NexradFile> nexradFile)
{
   std::shared_ptr<RadarProductRecord> record = nullptr;

   if (nexradFile != nullptr)
   {
      record = std::make_shared<RadarProductRecord>(nexradFile);
   }

   return record;
}

} // namespace types
} // namespace qt
} // namespace scwx
