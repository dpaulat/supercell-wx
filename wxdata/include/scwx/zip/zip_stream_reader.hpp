#pragma once

#include <cstdint>
#include <istream>
#include <memory>
#include <vector>

namespace scwx::zip
{

class ZipStreamReader
{
public:
   explicit ZipStreamReader(const std::string& filename);
   explicit ZipStreamReader(std::istream& stream);
   ~ZipStreamReader();

   ZipStreamReader(const ZipStreamReader&)            = delete;
   ZipStreamReader& operator=(const ZipStreamReader&) = delete;

   ZipStreamReader(ZipStreamReader&&) noexcept;
   ZipStreamReader& operator=(ZipStreamReader&&) noexcept;

   [[nodiscard]] bool       IsOpen() const;
   std::vector<std::string> ListFiles();
   bool ReadFile(const std::string& filename, std::string& output);
   bool ReadFile(const std::string&         filename,
                 std::vector<std::uint8_t>& output);
   void Close();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::zip
