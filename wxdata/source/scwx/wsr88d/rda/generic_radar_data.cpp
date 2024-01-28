#include <scwx/wsr88d/rda/generic_radar_data.hpp>
#include <scwx/util/logger.hpp>

namespace scwx
{
namespace wsr88d
{
namespace rda
{

static const std::string logPrefix_ = "scwx::wsr88d::rda::generic_radar_data";

static const std::unordered_map<std::string, DataBlockType> strToDataBlock_ {
   {"VOL", DataBlockType::Volume},
   {"ELV", DataBlockType::Elevation},
   {"RAD", DataBlockType::Radial},
   {"REF", DataBlockType::MomentRef},
   {"VEL", DataBlockType::MomentVel},
   {"SW ", DataBlockType::MomentSw},
   {"ZDR", DataBlockType::MomentZdr},
   {"PHI", DataBlockType::MomentPhi},
   {"RHO", DataBlockType::MomentRho},
   {"CFP", DataBlockType::MomentCfp}};

class GenericRadarData::MomentDataBlock::Impl
{
public:
   explicit Impl() {}
};

GenericRadarData::MomentDataBlock::MomentDataBlock() :
    p(std::make_unique<Impl>())
{
}
GenericRadarData::MomentDataBlock::~MomentDataBlock() = default;

GenericRadarData::MomentDataBlock::MomentDataBlock(MomentDataBlock&&) noexcept =
   default;
GenericRadarData::MomentDataBlock& GenericRadarData::MomentDataBlock::operator=(
   MomentDataBlock&&) noexcept = default;

class GenericRadarData::Impl
{
public:
   explicit Impl() {};
   ~Impl() = default;
};

GenericRadarData::GenericRadarData() :
    Level2Message(), p(std::make_unique<Impl>())
{
}
GenericRadarData::~GenericRadarData() = default;

GenericRadarData::GenericRadarData(GenericRadarData&&) noexcept = default;
GenericRadarData&
GenericRadarData::operator=(GenericRadarData&&) noexcept = default;

} // namespace rda
} // namespace wsr88d
} // namespace scwx
