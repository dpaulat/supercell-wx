#pragma once

#include <scwx/wsr88d/rpg/graphic_product_message.hpp>

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

class StormTrackingInformationMessage : public GraphicProductMessage
{
public:
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
