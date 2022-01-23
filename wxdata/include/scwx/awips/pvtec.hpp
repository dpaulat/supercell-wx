#pragma once

#include <scwx/awips/phenomenon.hpp>
#include <scwx/awips/significance.hpp>

#include <chrono>
#include <memory>

namespace scwx
{
namespace awips
{

class PVtecImpl;

class PVtec
{
public:
   enum class ProductType
   {
      Operational,
      Test,
      Experimental,
      OperationalWithExperimentalVtec,
      Unknown
   };

   enum class Action
   {
      New,
      Continued,
      ExtendedInArea,
      ExtendedInTime,
      ExtendedInAreaAndTime,
      Upgraded,
      Canceled,
      Expired,
      Routine,
      Correction,
      Unknown
   };

   explicit PVtec();
   ~PVtec();

   PVtec(const PVtec&) = delete;
   PVtec& operator=(const PVtec&) = delete;

   PVtec(PVtec&&) noexcept;
   PVtec& operator=(PVtec&&) noexcept;

   ProductType  fixed_identifier() const;
   Action       action() const;
   std::string  office_id() const;
   Phenomenon   phenomenon() const;
   Significance significance() const;
   int16_t      event_tracking_number() const;

   std::chrono::system_clock::time_point event_begin() const;
   std::chrono::system_clock::time_point event_end() const;

   bool Parse(const std::string& s);

   static ProductType GetProductType(const std::string& code);
   static std::string GetProductTypeCode(ProductType productType);

   static Action      GetAction(const std::string& code);
   static std::string GetActionCode(Action action);

private:
   std::unique_ptr<PVtecImpl> p;
};

} // namespace awips
} // namespace scwx
