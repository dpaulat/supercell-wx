#pragma once

#include <scwx/wsr88d/rda/generic_radar_data.hpp>

namespace scwx
{
namespace wsr88d
{
namespace rda
{

class DigitalRadarDataGeneric : public GenericRadarData
{
public:
   class DataBlock;
   class ElevationDataBlock;
   class MomentDataBlock;
   class RadialDataBlock;
   class VolumeDataBlock;

   explicit DigitalRadarDataGeneric();
   ~DigitalRadarDataGeneric();

   DigitalRadarDataGeneric(const DigitalRadarDataGeneric&)            = delete;
   DigitalRadarDataGeneric& operator=(const DigitalRadarDataGeneric&) = delete;

   DigitalRadarDataGeneric(DigitalRadarDataGeneric&&) noexcept;
   DigitalRadarDataGeneric& operator=(DigitalRadarDataGeneric&&) noexcept;

   std::string           radar_identifier() const;
   std::uint32_t         collection_time() const;
   std::uint16_t         modified_julian_date() const;
   std::uint16_t         azimuth_number() const;
   units::degrees<float> azimuth_angle() const;
   std::uint8_t          compression_indicator() const;
   std::uint16_t         radial_length() const;
   std::uint8_t          azimuth_resolution_spacing() const;
   std::uint8_t          radial_status() const;
   std::uint8_t          elevation_number() const;
   std::uint8_t          cut_sector_number() const;
   units::degrees<float> elevation_angle() const;
   std::uint8_t          radial_spot_blanking_status() const;
   std::uint8_t          azimuth_indexing_mode() const;
   std::uint16_t         data_block_count() const;
   std::uint16_t         volume_coverage_pattern_number() const;

   std::shared_ptr<ElevationDataBlock> elevation_data_block() const;
   std::shared_ptr<RadialDataBlock>    radial_data_block() const;
   std::shared_ptr<VolumeDataBlock>    volume_data_block() const;
   std::shared_ptr<GenericRadarData::MomentDataBlock>
   moment_data_block(DataBlockType type) const;

   bool Parse(std::istream& is);

   static std::shared_ptr<DigitalRadarDataGeneric>
   Create(Level2MessageHeader&& header, std::istream& is);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

class DigitalRadarDataGeneric::DataBlock
{
protected:
   explicit DataBlock(const std::string& dataBlockType,
                      const std::string& dataName);
   virtual ~DataBlock();

   DataBlock(const DataBlock&)            = delete;
   DataBlock& operator=(const DataBlock&) = delete;

   DataBlock(DataBlock&&) noexcept;
   DataBlock& operator=(DataBlock&&) noexcept;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

class DigitalRadarDataGeneric::ElevationDataBlock : public DataBlock
{
public:
   explicit ElevationDataBlock(const std::string& dataBlockType,
                               const std::string& dataName);
   ~ElevationDataBlock();

   ElevationDataBlock(const ElevationDataBlock&)            = delete;
   ElevationDataBlock& operator=(const ElevationDataBlock&) = delete;

   ElevationDataBlock(ElevationDataBlock&&) noexcept;
   ElevationDataBlock& operator=(ElevationDataBlock&&) noexcept;

   static std::shared_ptr<ElevationDataBlock>
   Create(const std::string& dataBlockType,
          const std::string& dataName,
          std::istream&      is);

private:
   class Impl;
   std::unique_ptr<Impl> p;

   bool Parse(std::istream& is);
};

class DigitalRadarDataGeneric::MomentDataBlock :
    public DataBlock,
    public GenericRadarData::MomentDataBlock
{
public:
   explicit MomentDataBlock(const std::string& dataBlockType,
                            const std::string& dataName);
   ~MomentDataBlock();

   MomentDataBlock(const MomentDataBlock&)            = delete;
   MomentDataBlock& operator=(const MomentDataBlock&) = delete;

   MomentDataBlock(MomentDataBlock&&) noexcept;
   MomentDataBlock& operator=(MomentDataBlock&&) noexcept;

   std::uint16_t            number_of_data_moment_gates() const;
   units::kilometers<float> data_moment_range() const;
   std::uint16_t            data_moment_range_raw() const;
   units::kilometers<float> data_moment_range_sample_interval() const;
   std::uint16_t            data_moment_range_sample_interval_raw() const;
   float                    snr_threshold() const;
   std::int16_t             snr_threshold_raw() const;
   std::uint8_t             data_word_size() const;
   float                    scale() const;
   float                    offset() const;
   const void*              data_moments() const;

   static std::shared_ptr<MomentDataBlock>
   Create(const std::string& dataBlockType,
          const std::string& dataName,
          std::istream&      is);

private:
   class Impl;
   std::unique_ptr<Impl> p;

   bool Parse(std::istream& is);
};

class DigitalRadarDataGeneric::RadialDataBlock : public DataBlock
{
public:
   explicit RadialDataBlock(const std::string& dataBlockType,
                            const std::string& dataName);
   ~RadialDataBlock();

   RadialDataBlock(const RadialDataBlock&)            = delete;
   RadialDataBlock& operator=(const RadialDataBlock&) = delete;

   RadialDataBlock(RadialDataBlock&&) noexcept;
   RadialDataBlock& operator=(RadialDataBlock&&) noexcept;

   float unambiguous_range() const;

   static std::shared_ptr<RadialDataBlock>
   Create(const std::string& dataBlockType,
          const std::string& dataName,
          std::istream&      is);

private:
   class Impl;
   std::unique_ptr<Impl> p;

   bool Parse(std::istream& is);
};

class DigitalRadarDataGeneric::VolumeDataBlock : public DataBlock
{
public:
   explicit VolumeDataBlock(const std::string& dataBlockType,
                            const std::string& dataName);
   ~VolumeDataBlock();

   VolumeDataBlock(const VolumeDataBlock&)            = delete;
   VolumeDataBlock& operator=(const VolumeDataBlock&) = delete;

   VolumeDataBlock(VolumeDataBlock&&) noexcept;
   VolumeDataBlock& operator=(VolumeDataBlock&&) noexcept;

   float         latitude() const;
   float         longitude() const;
   std::uint16_t volume_coverage_pattern_number() const;

   static std::shared_ptr<VolumeDataBlock>
   Create(const std::string& dataBlockType,
          const std::string& dataName,
          std::istream&      is);

private:
   class Impl;
   std::unique_ptr<Impl> p;

   bool Parse(std::istream& is);
};

} // namespace rda
} // namespace wsr88d
} // namespace scwx
