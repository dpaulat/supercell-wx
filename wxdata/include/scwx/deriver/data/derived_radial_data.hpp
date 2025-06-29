#pragma once
#include <scwx/deriver/data/derived_data.hpp>

#include <cstddef>
#include <memory>
#include <vector>
#include <chrono>

#include <units/angle.h>
#include <units/length.h>

namespace scwx::deriver::data
{

struct DerivedRadialMetaData
{
public:
   bool                          hasElevation = false;
   units::angle::degrees<double> elevation = units::angle::degrees<double>(0);

   float                            scale  = 0;
   float                            offset = 0;
   units::length::kilometers<float> range  = units::length::meters<float>(0);
   units::length::meters<float>     dataMomentInterval =
      units::length::meters<float>(0);
   std::chrono::system_clock::time_point sweepTime = {};
   uint16_t                              vcp       = 0;
   uint8_t                               threshold = 2;
   // NOLINTNEXTLINE 256 is a resonable guess for a uint8_t
   uint16_t numberOfLevels = 256;
};

class DerivedRadialData : public DerivedData
{
public:
   explicit DerivedRadialData(size_t radials, size_t gates);
   ~DerivedRadialData() override;

   DerivedRadialData(const DerivedRadialData&)            = delete;
   DerivedRadialData(DerivedRadialData&&)                 = delete;
   DerivedRadialData& operator=(const DerivedRadialData&) = delete;
   DerivedRadialData& operator=(DerivedRadialData&&)      = delete;

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
    * Sets the start and delta angle for a given radial
    *
    * @param radial The radial number of the bin
    * @param startAngle The starting angle of the radial
    * @param deltaAngle The change in the angle of the radial
    */
   void SetRadial(size_t radial, float startAngle, float deltaAngle);

   /**
    * Gets the levels for a given radial
    *
    * @param radial The radial number to get
    * @return The levels of the radial
    */
   std::vector<uint8_t>& levels(size_t radial);

   /**
    * Gets the start angle for the radial
    *
    * @param radial The radial number of the bin
    * @return The start angle for the radial
    */
   float start_angle(size_t radial);

   /**
    * Gets the delta angle for the radial
    *
    * @param radial The radial number of the bin
    * @return The delta angle for the radial
    */
   float delta_angle(size_t radial);

   DerivedRadialMetaData& meta_data();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::deriver::data
