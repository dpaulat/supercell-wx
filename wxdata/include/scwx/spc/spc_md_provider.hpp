#pragma once

#include <scwx/spc/spc_md_types.hpp>

#include <memory>
#include <string>

#include <boost/outcome/result.hpp>

namespace scwx::spc
{

class SpcMdProvider
{
public:
   SpcMdProvider();
   ~SpcMdProvider();
   SpcMdProvider(const SpcMdProvider&)            = delete;
   SpcMdProvider& operator=(const SpcMdProvider&) = delete;
   SpcMdProvider(SpcMdProvider&&) noexcept;
   SpcMdProvider& operator=(SpcMdProvider&&) noexcept;

   static boost::outcome_v2::result<MdData> FetchActiveMDs();

   static const std::string& kMdUrl();

private:
   static MdData      ParseKml(const std::string& kmlContent);
   static std::string FetchMdDiscussionText(int mdNumber);

   friend class SpcMdProviderTest;

   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::spc
