#include <scwx/deriver/data/derived_radial_data.hpp>
#include <vector>

namespace scwx::deriver::data
{

class DerivedRadialData::Impl
{
public:
   explicit Impl(size_t radials, size_t gates) :
       metaData_ {},
       radials_ {radials},
       gates_ {gates},
       values_ {radials, std::vector<uint8_t>(gates, 0)}
   {
      startAngles_ = std::vector<float>(radials, 0);
      deltaAngles_ = std::vector<float>(radials, 0);
   }

   DerivedRadialMetaData metaData_;

   size_t radials_;
   size_t gates_;

   std::vector<float>                startAngles_;
   std::vector<float>                deltaAngles_;
   std::vector<std::vector<uint8_t>> values_;
};

DerivedRadialData::DerivedRadialData(size_t radials, size_t gates) :
    p {std::make_unique<Impl>(radials, gates)}
{
}

DerivedRadialData::~DerivedRadialData() = default;

void DerivedRadialData::SetRadial(size_t radial,
                                  float  startAngle,
                                  float  deltaAngle)
{
   p->startAngles_.at(radial) = startAngle;
   p->deltaAngles_.at(radial) = deltaAngle;
}

size_t DerivedRadialData::radials() const
{
   return p->radials_;
}

size_t DerivedRadialData::gates() const
{
   return p->gates_;
}

std::vector<uint8_t>& DerivedRadialData::levels(size_t radial)
{
   return p->values_.at(radial);
}

float DerivedRadialData::start_angle(size_t radial)
{
   return p->startAngles_.at(radial);
}

float DerivedRadialData::delta_angle(size_t radial)
{
   return p->deltaAngles_.at(radial);
}

DerivedRadialMetaData& DerivedRadialData::meta_data()
{
   return p->metaData_;
}

} // namespace scwx::deriver::data
