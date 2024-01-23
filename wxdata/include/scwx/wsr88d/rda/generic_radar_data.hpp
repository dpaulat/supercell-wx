#pragma once

#include <scwx/util/iterator.hpp>
#include <scwx/wsr88d/rda/level2_message.hpp>

#include <units/angle.h>
#include <units/length.h>

namespace scwx
{
namespace wsr88d
{
namespace rda
{

enum class DataBlockType
{
   Volume,
   Elevation,
   Radial,
   MomentRef,
   MomentVel,
   MomentSw,
   MomentZdr,
   MomentPhi,
   MomentRho,
   MomentCfp,
   Unknown
};
typedef util::
   Iterator<DataBlockType, DataBlockType::MomentRef, DataBlockType::MomentCfp>
      MomentDataBlockTypeIterator;

class GenericRadarData;

typedef std::map<std::uint16_t, std::shared_ptr<GenericRadarData>>
   ElevationScan;

class GenericRadarData : public Level2Message
{
public:
   class MomentDataBlock;

   explicit GenericRadarData();
   virtual ~GenericRadarData();

   GenericRadarData(const GenericRadarData&)            = delete;
   GenericRadarData& operator=(const GenericRadarData&) = delete;

   GenericRadarData(GenericRadarData&&) noexcept;
   GenericRadarData& operator=(GenericRadarData&&) noexcept;

   virtual std::uint32_t         collection_time() const                = 0;
   virtual std::uint16_t         modified_julian_date() const           = 0;
   virtual units::degrees<float> azimuth_angle() const                  = 0;
   virtual std::uint16_t         volume_coverage_pattern_number() const = 0;

   virtual std::shared_ptr<MomentDataBlock>
   moment_data_block(DataBlockType type) const = 0;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

class GenericRadarData::MomentDataBlock
{
public:
   explicit MomentDataBlock();
   virtual ~MomentDataBlock();

   MomentDataBlock(const MomentDataBlock&)            = delete;
   MomentDataBlock& operator=(const MomentDataBlock&) = delete;

   MomentDataBlock(MomentDataBlock&&) noexcept;
   MomentDataBlock& operator=(MomentDataBlock&&) noexcept;

   virtual std::uint16_t            number_of_data_moment_gates() const = 0;
   virtual units::kilometers<float> data_moment_range() const           = 0;
   virtual std::uint16_t            data_moment_range_raw() const       = 0;
   virtual units::kilometers<float>
                         data_moment_range_sample_interval() const     = 0;
   virtual std::uint16_t data_moment_range_sample_interval_raw() const = 0;
   virtual std::int16_t  snr_threshold_raw() const                     = 0;
   virtual std::uint8_t  data_word_size() const                        = 0;
   virtual float         scale() const                                 = 0;
   virtual float         offset() const                                = 0;
   virtual const void*   data_moments() const                          = 0;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace rda
} // namespace wsr88d
} // namespace scwx
