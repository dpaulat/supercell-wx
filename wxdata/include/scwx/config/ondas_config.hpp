#pragma once

#include <chrono>
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

   [[nodiscard]] std::string              list_file() const;
   [[nodiscard]] std::vector<std::string> sites() const;
   [[nodiscard]] std::vector<std::string> products() const;

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
   [[nodiscard]] std::string
   ApplySiteSubstitution(const std::string& radarSite,
                         const std::string& product) const;

   /**
    * @brief Get time point from filename
    *
    * @param filename
    * @return std::chrono::system_clock::time_point
    */
   static std::chrono::system_clock::time_point
   GetTimePointFromFilename(const std::string& filename);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::config
