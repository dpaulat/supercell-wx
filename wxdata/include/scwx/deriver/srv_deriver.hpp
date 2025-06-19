#pragma once
#include <scwx/deriver/base_deriver.hpp>

namespace scwx::deriver
{

class SrvDeriver : public BaseDeriver
{
public:
   explicit SrvDeriver();
   ~SrvDeriver() override;

   SrvDeriver(const SrvDeriver&)            = delete;
   SrvDeriver(SrvDeriver&&)                 = delete;
   SrvDeriver& operator=(const SrvDeriver&) = delete;
   SrvDeriver& operator=(SrvDeriver&&)      = delete;

   std::shared_ptr<data::DerivedData>
   GetOutput(const std::string& product) override;

   // TODO put this in cpp
   static const std::unordered_map<std::string, DerivedProductInfo>&
   deriveable_products()
   {
      static std::unordered_map<std::string, DerivedProductInfo>
         derivableProducts_ = {
            {"SRV-AVG-X", {"SRV-AVG-X", {"NXG", "N0S"}, {}}},
            {"SRV-AVG-Y", {"SRV-AVG-Y", {"NYG", "N0S"}, {}}},
            {"SRV-AVG-Z", {"SRV-AVG-Z", {"NZG", "N0S"}, {}}},
            {"SRV-AVG-0", {"SRV-AVG-0", {"N0G", "N0S"}, {}}},
            {"SRV-AVG-A", {"SRV-AVG-A", {"NAG", "N0S"}, {}}},
            {"SRV-AVG-1", {"SRV-AVG-1", {"N1G", "N0S"}, {}}},
            {"SRV-AVG-B", {"SRV-AVG-B", {"NBG", "N0S"}, {}}},
            {"SRV-AVG-2", {"SRV-AVG-2", {"N2G", "N0S"}, {}}},
            {"SRV-AVG-3", {"SRV-AVG-3", {"N3G", "N0S"}, {}}},
         };
      return derivableProducts_;
   }

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::deriver
