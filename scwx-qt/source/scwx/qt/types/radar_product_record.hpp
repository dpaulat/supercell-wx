#pragma once

#include <scwx/common/products.hpp>
#include <scwx/wsr88d/ar2v_file.hpp>
#include <scwx/wsr88d/level3_file.hpp>

#include <chrono>
#include <memory>

namespace scwx
{
namespace qt
{
namespace types
{

class RadarProductRecordImpl;

class RadarProductRecord
{
public:
   explicit RadarProductRecord(std::shared_ptr<wsr88d::NexradFile> nexradFile);
   ~RadarProductRecord();

   RadarProductRecord(const RadarProductRecord&) = delete;
   RadarProductRecord& operator=(const RadarProductRecord&) = delete;

   RadarProductRecord(RadarProductRecord&&) noexcept;
   RadarProductRecord& operator=(RadarProductRecord&&) noexcept;

   std::shared_ptr<wsr88d::Ar2vFile>     level2_file() const;
   std::shared_ptr<wsr88d::Level3File>   level3_file() const;
   std::shared_ptr<wsr88d::NexradFile>   nexrad_file() const;
   int16_t                               product_code() const;
   std::string                           radar_id() const;
   std::string                           radar_product() const;
   common::RadarProductGroup             radar_product_group() const;
   std::string                           site_id() const;
   std::chrono::system_clock::time_point time() const;

   static std::shared_ptr<RadarProductRecord>
   Create(std::shared_ptr<wsr88d::NexradFile> nexradFile);

private:
   std::unique_ptr<RadarProductRecordImpl> p;
};

} // namespace types
} // namespace qt
} // namespace scwx
