#pragma once

#include <memory>
#include <string>
#include <vector>

namespace scwx
{
namespace awips
{

class UgcImpl;

class Ugc
{
public:
   explicit Ugc();
   ~Ugc();

   Ugc(const Ugc&)            = delete;
   Ugc& operator=(const Ugc&) = delete;

   Ugc(Ugc&&) noexcept;
   Ugc& operator=(Ugc&&) noexcept;

   std::vector<std::string> states() const;
   std::vector<std::string> fips_ids() const;
   std::string              product_expiration() const;

   bool Parse(const std::vector<std::string>& ugcString);

private:
   std::unique_ptr<UgcImpl> p;
};

} // namespace awips
} // namespace scwx
