#pragma once

#include <scwx/grib2/grib2_types.hpp>

#include <memory>
#include <optional>
#include <vector>

namespace scwx::grib2
{

class Grib2Message
{
public:
   explicit Grib2Message(const std::vector<char>& data);
   ~Grib2Message();

   Grib2Message(const Grib2Message&)            = delete;
   Grib2Message& operator=(const Grib2Message&) = delete;

   Grib2Message(Grib2Message&&) noexcept;
   Grib2Message& operator=(Grib2Message&&) noexcept;

   bool IsValid() const;

   Discipline   discipline() const;
   ParameterId  parameter() const;
   SurfaceType  surfaceType() const;
   double       levelValue() const;
   double       forecastHour() const;
   std::optional<GridDef>       gridDefinition() const;
   std::optional<SimplePacking> simplePacking() const;

   std::vector<double> values() const;
   double valueAt(std::size_t i, std::size_t j) const;
   std::size_t dataPointCount() const;
   const std::string& errorMessage() const;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::grib2
