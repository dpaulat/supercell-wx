#pragma once
#include <scwx/derivers/data/derived_data.hpp>

#include <scwx/wsr88d/wsr88d_types.hpp>

#include <cstddef>
#include <memory>
#include <optional>

namespace scwx::derivers::data
{

class DerivedRadialData : public DerivedData
{
public:
   explicit DerivedRadialData(size_t radials, size_t gates);
   ~DerivedRadialData();

   DerivedRadialData(const DerivedRadialData&)            = delete;
   DerivedRadialData(DerivedRadialData&&)                 = delete;
   DerivedRadialData& operator=(const DerivedRadialData&) = delete;
   DerivedRadialData& operator=(DerivedRadialData&&)      = delete;

   [[nodiscard]] size_t radials() const;
   [[nodiscard]] size_t gates() const;

   void set_bin(size_t                               radial,
                size_t                               gate,
                float                                value,
                std::optional<wsr88d::DataLevelCode> code);
   void set_radial(size_t radial, float startAngle, float deltaAngle);

   std::optional<wsr88d::DataLevelCode> get_code(size_t radial, size_t gate);
   float                                get_value(size_t radial, size_t gate);
   float get_start_angle(size_t radial);
   float get_delta_angle(size_t radial);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::derivers::data
