#include <scwx/deriver/deriver_factory.hpp>
#include <scwx/deriver/srv_deriver.hpp>

namespace scwx::deriver
{

static const std::vector<std::string> kDerivedProductCategories_ = {
   "SRV",
};

static const std::unordered_map<std::string, std::vector<std::string>>
   kDerivedProductsInCategories_ = {{"SRV", {"Average Storm Velocity"}}};

static const std::unordered_map<std::string, std::vector<std::string>>
   kDerivedTiltsInProduct_ = {{"Average Storm Velocity",
                               {
                                  "SRV-AVG-X",
                                  "SRV-AVG-Y",
                                  "SRV-AVG-Z",
                                  "SRV-AVG-0",
                                  "SRV-AVG-A",
                                  "SRV-AVG-1",
                                  "SRV-AVG-B",
                                  "SRV-AVG-2",
                                  "SRV-AVG-3",
                               }}};

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

const std::vector<std::string>& DeriverFactory::GetDerivedProductCategories()
{
   return kDerivedProductCategories_;
}

const std::vector<std::string>&
DeriverFactory::GetDerivedProductsInCategory(const std::string& category)
{
   static const std::vector<std::string> empty = {};
   const auto& productsIt = kDerivedProductsInCategories_.find(category);
   if (productsIt == kDerivedProductsInCategories_.cend())
   {
      return empty;
   }
   return productsIt->second;
}

const std::vector<std::string>&
DeriverFactory::GetDerivedTiltsForProducts(const std::string& category)
{
   static const std::vector<std::string> empty = {};
   const auto& tiltsIt = kDerivedTiltsInProduct_.find(category);
   if (tiltsIt == kDerivedTiltsInProduct_.cend())
   {
      return empty;
   }
   return tiltsIt->second;
}

} // namespace scwx::deriver
