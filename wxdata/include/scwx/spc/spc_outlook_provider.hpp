#pragma once

#include <scwx/spc/spc_types.hpp>

#include <memory>
#include <string>

#include <boost/outcome/result.hpp>

namespace scwx::spc
{

class SpcOutlookProvider
{
public:
   SpcOutlookProvider();
   ~SpcOutlookProvider();
   SpcOutlookProvider(const SpcOutlookProvider&)            = delete;
   SpcOutlookProvider& operator=(const SpcOutlookProvider&) = delete;
   SpcOutlookProvider(SpcOutlookProvider&&) noexcept;
   SpcOutlookProvider& operator=(SpcOutlookProvider&&) noexcept;

   static boost::outcome_v2::result<OutlookData>
   FetchOutlook(OutlookDay day, OutlookProduct product);

   static std::string GetOutlookUrl(OutlookDay day, OutlookProduct product);

private:
   class Impl;
   std::unique_ptr<Impl> p;

   static const std::string kBaseUrl_;
};

} // namespace scwx::spc
