#pragma once

#include <scwx/deriver/base_deriver.hpp>
#include <memory>

namespace scwx::deriver
{
class DeriverFactory
{
public:
   explicit DeriverFactory() = delete;
   ~DeriverFactory()         = delete;

   DeriverFactory(const DeriverFactory&)            = delete;
   DeriverFactory& operator=(const DeriverFactory&) = delete;

   DeriverFactory(DeriverFactory&&) noexcept            = delete;
   DeriverFactory& operator=(DeriverFactory&&) noexcept = delete;

   static std::shared_ptr<BaseDeriver>
   CreateDeriver(const std::string& product);

   // TODO this is named poorly
   static const std::unordered_map<std::string, DerivedProductInfo>&
   GetDeriveableProducts(const std::string& product);

   static const std::vector<std::string>& GetDerivedProductCategories();
   static const std::vector<std::string>&
   GetDerivedProductsInCategory(const std::string& category);
   static const std::vector<std::string>&
   GetDerivedTiltsForProducts(const std::string& product);
};

} // namespace scwx::deriver
