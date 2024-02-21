#pragma once

#include <scwx/wsr88d/rpg/graphic_product_message.hpp>

#include <optional>

#include <units/angle.h>
#include <units/length.h>
#include <units/velocity.h>

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

class StormTrackingInformationMessage : public GraphicProductMessage
{
public:
   struct StiRecord
   {
      struct Position
      {
         std::optional<units::angle::degrees<float>>         azimuth_ {};
         std::optional<units::length::nautical_miles<float>> range_ {};
      };

      Position                                     currentPosition_ {};
      std::optional<units::angle::degrees<float>>  direction_;
      std::optional<units::velocity::knots<float>> speed_;

      std::array<Position, 4> forecastPosition_ {};

      std::optional<units::length::nautical_miles<float>> forecastError_ {};
      std::optional<units::length::nautical_miles<float>> meanError_ {};

      std::optional<std::int16_t>               maxDbz_ {};
      std::optional<units::length::feet<float>> maxDbzHeight_ {};
   };

   explicit StormTrackingInformationMessage();
   ~StormTrackingInformationMessage();

   StormTrackingInformationMessage(const StormTrackingInformationMessage&) =
      delete;
   StormTrackingInformationMessage&
   operator=(const StormTrackingInformationMessage&) = delete;

   StormTrackingInformationMessage(StormTrackingInformationMessage&&) noexcept;
   StormTrackingInformationMessage&
   operator=(StormTrackingInformationMessage&&) noexcept;

   bool Parse(std::istream& is) override;

   static std::shared_ptr<StormTrackingInformationMessage>
   Create(Level3MessageHeader&& header, std::istream& is);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
