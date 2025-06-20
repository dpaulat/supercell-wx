#pragma once

#include <scwx/deriver/base_deriver.hpp>
#include <memory>

namespace scwx::deriver
{
class DeriverFactory
{
private:
   explicit DeriverFactory() = delete;
   ~DeriverFactory()         = delete;

   DeriverFactory(const DeriverFactory&)            = delete;
   DeriverFactory& operator=(const DeriverFactory&) = delete;

   DeriverFactory(DeriverFactory&&) noexcept            = delete;
   DeriverFactory& operator=(DeriverFactory&&) noexcept = delete;

public:
   static std::shared_ptr<BaseDeriver>
   CreateDeriver(const std::string& product);
   static const std::unordered_map<std::string, DerivedProductInfo>&
   GetDeriveableProducts(const std::string& product);
};

} // namespace scwx::deriver
