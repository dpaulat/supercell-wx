#pragma once
#include <scwx/deriver/base_deriver.hpp>

namespace scwx::deriver
{

class KdpDeriver : public BaseDeriver
{
public:
   explicit KdpDeriver();
   ~KdpDeriver() override;

   KdpDeriver(const KdpDeriver&)            = delete;
   KdpDeriver(KdpDeriver&&)                 = delete;
   KdpDeriver& operator=(const KdpDeriver&) = delete;
   KdpDeriver& operator=(KdpDeriver&&)      = delete;

   std::shared_ptr<data::DerivedData>
   GetOutput(const std::string& product) override;

   static const std::unordered_map<std::string, DerivedProductInfo>&
   deriveable_products();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::deriver
