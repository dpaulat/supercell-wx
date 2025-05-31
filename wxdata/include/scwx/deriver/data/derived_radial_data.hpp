#pragma once
#include <scwx/deriver/data/derived_data.hpp>

#include <scwx/wsr88d/wsr88d_types.hpp>

#include <cstddef>
#include <memory>
#include <optional>

namespace scwx::deriver::data
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

   // TODO things such as gate width, range, and elevation all need to be
   // brough into this class

   /**
    * The number of radials that this data has
    *
    * @param file The number of radials that this data has
    */
   [[nodiscard]] size_t radials() const;

   /**
    * The number of gates that this data has
    *
    * @param file The number of gates that this data has
    */
   [[nodiscard]] size_t gates() const;

   /**
    * Sets the value and code for a given bin
    *
    * @param radial The radial number of the bin
    * @param gate The gate number of the bin
    * @param value The value the bin will have
    * @param code The code the bin will have
    */
   void SetBin(size_t                               radial,
               size_t                               gate,
               float                                value,
               std::optional<wsr88d::DataLevelCode> code);

   /**
    * Sets the start and delta angle for a given radial
    *
    * @param radial The radial number of the bin
    * @param startAngle The starting angle of the radial
    * @param deltaAngle The change in the angle of the radial
    */
   void SetRadial(size_t radial, float startAngle, float deltaAngle);

   /**
    * Gets the code for a given bin
    *
    * @param radial The radial number of the bin
    * @param gate The gate number of the bin
    * @return code The code of the bin
    */
   std::optional<wsr88d::DataLevelCode> GetCode(size_t radial, size_t gate);

   /**
    * Gets the value for a given bin
    *
    * @param radial The radial number of the bin
    * @param gate The gate number of the bin
    * @return value The value of the bin
    */
   float GetValue(size_t radial, size_t gate);

   /**
    * Gets the start angle for the radial
    *
    * @param radial The radial number of the bin
    * @return The start angle for the radial
    */
   float GetStartAngle(size_t radial);

   /**
    * Gets the delta angle for the radial
    *
    * @param radial The radial number of the bin
    * @return The delta angle for the radial
    */
   float GetDeltaAngle(size_t radial);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::deriver::data
