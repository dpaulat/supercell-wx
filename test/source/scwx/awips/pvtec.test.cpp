#include <scwx/awips/pvtec.hpp>

#include <gtest/gtest.h>

#include <boost/log/trivial.hpp>

namespace scwx
{
namespace awips
{

static const std::string logPrefix_ = "[scwx::awips::pvtec.test] ";

std::pair<std::chrono::year_month_day,
          std::chrono::hh_mm_ss<std::chrono::minutes>>
GetDateTime(std::chrono::system_clock::time_point t);

TEST(PVtec, FlashFloodWarningExtended)
{
   using namespace std::chrono;

   PVtec       pvtec;
   std::string s = "/O.EXT.KJAN.FF.W.0023.000000T0000Z-210606T1700Z/";

   pvtec.Parse(s);

   auto eventBegin = GetDateTime(pvtec.event_begin());
   auto eventEnd   = GetDateTime(pvtec.event_end());

   EXPECT_EQ(pvtec.fixed_identifier(), PVtec::ProductType::Operational);
   EXPECT_EQ(pvtec.action(), PVtec::Action::ExtendedInTime);
   EXPECT_EQ(pvtec.office_id(), "KJAN");
   EXPECT_EQ(pvtec.phenomenon(), Phenomenon::FlashFlood);
   EXPECT_EQ(pvtec.significance(), Significance::Warning);
   EXPECT_EQ(pvtec.event_tracking_number(), 23);

   EXPECT_EQ(pvtec.event_begin(), std::chrono::system_clock::time_point {});
   EXPECT_EQ(eventBegin.first.year(), 1970y);
   EXPECT_EQ(eventBegin.first.month(), month {1});
   EXPECT_EQ(eventBegin.first.day(), 1d);
   EXPECT_EQ(eventBegin.second.hours(), 0h);
   EXPECT_EQ(eventBegin.second.minutes(), 0min);

   EXPECT_EQ(eventEnd.first.year(), 2021y);
   EXPECT_EQ(eventEnd.first.month(), month {6});
   EXPECT_EQ(eventEnd.first.day(), 6d);
   EXPECT_EQ(eventEnd.second.hours(), 17h);
   EXPECT_EQ(eventEnd.second.minutes(), 0min);
}

TEST(PVtec, TornadoWarningNew)
{
   using namespace std::chrono;

   PVtec       pvtec;
   std::string s = "/O.NEW.KLIX.TO.W.0032.210606T1501Z-210606T1600Z/";

   pvtec.Parse(s);

   auto eventBegin = GetDateTime(pvtec.event_begin());
   auto eventEnd   = GetDateTime(pvtec.event_end());

   EXPECT_EQ(pvtec.fixed_identifier(), PVtec::ProductType::Operational);
   EXPECT_EQ(pvtec.action(), PVtec::Action::New);
   EXPECT_EQ(pvtec.office_id(), "KLIX");
   EXPECT_EQ(pvtec.phenomenon(), Phenomenon::Tornado);
   EXPECT_EQ(pvtec.significance(), Significance::Warning);
   EXPECT_EQ(pvtec.event_tracking_number(), 32);

   EXPECT_EQ(eventBegin.first.year(), 2021y);
   EXPECT_EQ(eventBegin.first.month(), month {6});
   EXPECT_EQ(eventBegin.first.day(), 6d);
   EXPECT_EQ(eventBegin.second.hours(), 15h);
   EXPECT_EQ(eventBegin.second.minutes(), 1min);

   EXPECT_EQ(eventEnd.first.year(), 2021y);
   EXPECT_EQ(eventEnd.first.month(), month {6});
   EXPECT_EQ(eventEnd.first.day(), 6d);
   EXPECT_EQ(eventEnd.second.hours(), 16h);
   EXPECT_EQ(eventEnd.second.minutes(), 0min);
}

TEST(PVtec, TornadoWarningContinued)
{
   using namespace std::chrono;

   PVtec       pvtec;
   std::string s = "/O.CON.KLIX.TO.W.0032.000000T0000Z-210606T1600Z/";

   pvtec.Parse(s);

   auto eventBegin = GetDateTime(pvtec.event_begin());
   auto eventEnd   = GetDateTime(pvtec.event_end());

   EXPECT_EQ(pvtec.fixed_identifier(), PVtec::ProductType::Operational);
   EXPECT_EQ(pvtec.action(), PVtec::Action::Continued);
   EXPECT_EQ(pvtec.office_id(), "KLIX");
   EXPECT_EQ(pvtec.phenomenon(), Phenomenon::Tornado);
   EXPECT_EQ(pvtec.significance(), Significance::Warning);
   EXPECT_EQ(pvtec.event_tracking_number(), 32);

   EXPECT_EQ(pvtec.event_begin(), std::chrono::system_clock::time_point {});
   EXPECT_EQ(eventBegin.first.year(), 1970y);
   EXPECT_EQ(eventBegin.first.month(), month {1});
   EXPECT_EQ(eventBegin.first.day(), 1d);
   EXPECT_EQ(eventBegin.second.hours(), 0h);
   EXPECT_EQ(eventBegin.second.minutes(), 0min);

   EXPECT_EQ(eventEnd.first.year(), 2021y);
   EXPECT_EQ(eventEnd.first.month(), month {6});
   EXPECT_EQ(eventEnd.first.day(), 6d);
   EXPECT_EQ(eventEnd.second.hours(), 16h);
   EXPECT_EQ(eventEnd.second.minutes(), 0min);
}

std::pair<std::chrono::year_month_day,
          std::chrono::hh_mm_ss<std::chrono::minutes>>
GetDateTime(std::chrono::system_clock::time_point t)
{
   using namespace std::chrono;

   auto tDays    = floor<days>(t);
   auto tDate    = year_month_day {tDays};
   auto tMinutes = floor<minutes>(t - tDays);
   auto tTime    = hh_mm_ss {tMinutes};

   return std::make_pair(tDate, tTime);
}

} // namespace awips
} // namespace scwx
