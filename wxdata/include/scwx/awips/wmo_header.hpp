#pragma once

#include <memory>
#include <string>

namespace scwx
{
namespace awips
{

class WmoHeaderImpl;

/**
 * @brief The WMO Header is defined in WMO Manual No. 386, with additional codes
 * defined in WMO Codes Manual 306.  The NWS summarizes the relevant
 * information.
 *
 * <https://www.roc.noaa.gov/WSR88D/Level_III/Level3Info.aspx>
 * <https://www.weather.gov/tg/head>
 * <https://www.weather.gov/tg/headef>
 * <https://www.weather.gov/tg/bbb>
 * <https://www.weather.gov/tg/awips>
 */
class WmoHeader
{
public:
   explicit WmoHeader();
   ~WmoHeader();

   WmoHeader(const WmoHeader&) = delete;
   WmoHeader& operator=(const WmoHeader&) = delete;

   WmoHeader(WmoHeader&&) noexcept;
   WmoHeader& operator=(WmoHeader&&) noexcept;

   const std::string& sequence_number() const;
   const std::string& data_type() const;
   const std::string& geographic_designator() const;
   const std::string& bulletin_id() const;
   const std::string& icao() const;
   const std::string& date_time() const;
   const std::string& bbb_indicator() const;
   const std::string& product_category() const;
   const std::string& product_designator() const;

   bool Parse(std::istream& is);

private:
   std::unique_ptr<WmoHeaderImpl> p;
};

} // namespace awips
} // namespace scwx
