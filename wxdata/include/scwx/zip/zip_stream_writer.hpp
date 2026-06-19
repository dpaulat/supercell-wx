#pragma once

#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

namespace scwx::zip
{

class ZipStreamWriter
{
public:
   explicit ZipStreamWriter(const std::string& filename);
   explicit ZipStreamWriter(std::iostream& stream);
   ~ZipStreamWriter();

   ZipStreamWriter(const ZipStreamWriter&)            = delete;
   ZipStreamWriter& operator=(const ZipStreamWriter&) = delete;

   ZipStreamWriter(ZipStreamWriter&&) noexcept;
   ZipStreamWriter& operator=(ZipStreamWriter&&) noexcept;

   [[nodiscard]] bool IsOpen() const;
   bool AddFile(const std::string& filename, const std::string& content);
   bool AddFile(const std::string&               filename,
                const std::vector<std::uint8_t>& content);
   void Close();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::zip
