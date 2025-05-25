#include <scwx/derivers/data/derived_radial_data.hpp>
#include <vector>

namespace scwx::derivers::data
{

class DerivedRadialData::Impl
{
public:
   explicit Impl(size_t radials, size_t gates) :
       radials_ {radials},
       gates_ {gates},
       values_ {radials, std::vector<float>(gates, 0)},
       codes_ {radials,
               std::vector<std::optional<wsr88d::DataLevelCode>>(gates,
                                                                 std::nullopt)}
   {
      startAngles_ = std::vector<float>(radials, 0);
      deltaAngles_ = std::vector<float>(radials, 0);
   }

   size_t radials_;
   size_t gates_;

   std::vector<float>                                             startAngles_;
   std::vector<float>                                             deltaAngles_;
   std::vector<std::vector<float>>                                values_;
   std::vector<std::vector<std::optional<wsr88d::DataLevelCode>>> codes_;
};

DerivedRadialData::DerivedRadialData(size_t radials, size_t gates) :
    p {std::make_unique<Impl>(radials, gates)}
{
}

DerivedRadialData::~DerivedRadialData() = default;

void DerivedRadialData::set_bin(size_t                               radial,
                                size_t                               gate,
                                float                                value,
                                std::optional<wsr88d::DataLevelCode> code)
{
   p->values_.at(radial).at(gate) = value;
   p->codes_.at(radial).at(gate)  = code;
}

void DerivedRadialData::set_radial(size_t radial,
                                   float  startAngle,
                                   float  deltaAngle)
{
   p->startAngles_.at(radial) = startAngle;
   p->deltaAngles_.at(radial) = deltaAngle;
}

std::optional<wsr88d::DataLevelCode> DerivedRadialData::get_code(size_t radial,
                                                                 size_t gate)
{
   return p->codes_.at(radial).at(gate);
}

float DerivedRadialData::get_value(size_t radial, size_t gate)
{
   return p->values_.at(radial).at(gate);
}

float DerivedRadialData::get_start_angle(size_t radial)
{
   return p->startAngles_.at(radial);
}

float DerivedRadialData::get_delta_angle(size_t radial)
{
   return p->deltaAngles_.at(radial);
}

} // namespace scwx::derivers::data
