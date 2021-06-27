#pragma once

namespace scwx
{
namespace wsr88d
{
namespace rda
{

enum class MessageId : uint8_t
{
   RdaStatusData              = 2,
   PerformanceMaintenanceData = 3,
   VolumeCoveragePatternData  = 5,
   ClutterFilterMap           = 15,
   RdaAdaptationData          = 18,
   DigitalRadarData           = 31
};

} // namespace rda
} // namespace wsr88d
} // namespace scwx
