#pragma once

namespace scwx
{
namespace wsr88d
{
namespace rda
{

enum class MessageId : std::uint8_t
{
   DigitalRadarData           = 1,
   RdaStatusData              = 2,
   PerformanceMaintenanceData = 3,
   VolumeCoveragePatternData  = 5,
   ClutterFilterMap           = 15,
   RdaAdaptationData          = 18,
   DigitalRadarDataGeneric    = 31
};

} // namespace rda
} // namespace wsr88d
} // namespace scwx
