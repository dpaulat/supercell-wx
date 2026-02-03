#pragma once

#include <istream>
#include <memory>
#include <string>
#include <vector>

namespace scwx::config
{

class OndasConfig
{
public:
   explicit OndasConfig();
   virtual ~OndasConfig();

   OndasConfig(const OndasConfig&)            = delete;
   OndasConfig& operator=(const OndasConfig&) = delete;

   OndasConfig(OndasConfig&&) noexcept;
   OndasConfig& operator=(OndasConfig&&) noexcept;

   [[nodiscard]] std::string list_file() const;
   [[nodiscard]] std::vector<std::string> sites() const;

   /**
    * @brief Parse config content string
    */
   void Parse(std::istream& is);

   /**
    * @brief Apply site substitution
    *
    * Converts SSSS/SSS/ssss/sss patterns to actual site ID
    * Example: "sss/N0R" with site "KILN" -> "iln/N0R"
    */
   std::string ApplySiteSubstitution(const std::string& radarSite,
                                     const std::string& product);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::config
