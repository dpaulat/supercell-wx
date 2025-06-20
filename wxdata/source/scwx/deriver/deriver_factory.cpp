#include <scwx/deriver/deriver_factory.hpp>
#include <scwx/deriver/srv_deriver.hpp>

namespace scwx::deriver
{

std::shared_ptr<BaseDeriver>
DeriverFactory::CreateDeriver(const std::string& product)
{
   if (product.starts_with("SRV-AVG"))
   {
      return std::make_shared<SrvDeriver>();
   }

   return nullptr;
}

const std::unordered_map<std::string, DerivedProductInfo>&
DeriverFactory::GetDeriveableProducts(const std::string& product)
{
   if (product.starts_with("SRV-AVG"))
   {
      return SrvDeriver::deriveable_products();
   }

   const static std::unordered_map<std::string, DerivedProductInfo> empty = {};
   return empty;
}

} // namespace scwx::deriver
