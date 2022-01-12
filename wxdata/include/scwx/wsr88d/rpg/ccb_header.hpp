#pragma once

#include <memory>
#include <string>

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

class CcbHeaderImpl;

/**
 * @brief The Communication Control Block Header is defined in the Interface
 * Control Document (ICD) for the National Weather Service Telecommunications
 * Gateway (NWSTG).
 *
 * <https://web.archive.org/web/20010706211204/http://www.nws.noaa.gov/pub/noaaport/nwstgicd.pdf>
 */
class CcbHeader
{
public:
   explicit CcbHeader();
   ~CcbHeader();

   CcbHeader(const CcbHeader&) = delete;
   CcbHeader& operator=(const CcbHeader&) = delete;

   CcbHeader(CcbHeader&&) noexcept;
   CcbHeader& operator=(CcbHeader&&) noexcept;

   uint16_t    ff() const;
   uint16_t    ccb_length() const;
   uint8_t     mode() const;
   uint8_t     submode() const;
   char        precedence() const;
   char        classification() const;
   std::string message_originator() const;
   uint8_t     category() const;
   uint8_t     subcategory() const;
   uint16_t    user_defined() const;
   uint8_t     year() const;
   uint8_t     month() const;
   uint8_t     tor_day() const;
   uint8_t     tor_hour() const;
   uint8_t     tor_minute() const;
   uint8_t     number_of_destinations() const;
   std::string message_destination(uint8_t i) const;

   bool Parse(std::istream& is);

private:
   std::unique_ptr<CcbHeaderImpl> p;
};

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
