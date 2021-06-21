#pragma once

#include <scwx/wsr88d/rda/message.hpp>

namespace scwx
{
namespace wsr88d
{
namespace rda
{

class DigitalRadarDataImpl;

class DigitalRadarData : public Message
{
public:
   explicit DigitalRadarData();
   ~DigitalRadarData();

   DigitalRadarData(const Message&) = delete;
   DigitalRadarData& operator=(const DigitalRadarData&) = delete;

   DigitalRadarData(DigitalRadarData&&) noexcept;
   DigitalRadarData& operator=(DigitalRadarData&&) noexcept;

   bool Parse(std::istream& is);

   static std::unique_ptr<DigitalRadarData> Create(MessageHeader&& header,
                                                   std::istream&   is);

private:
   std::unique_ptr<DigitalRadarDataImpl> p;
};

} // namespace rda
} // namespace wsr88d
} // namespace scwx
