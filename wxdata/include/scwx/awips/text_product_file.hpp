#pragma once

#include <scwx/awips/text_product_message.hpp>

#include <memory>
#include <string>

namespace scwx
{
namespace awips
{

class TextProductFileImpl;

class TextProductFile
{
public:
   explicit TextProductFile();
   ~TextProductFile();

   TextProductFile(const TextProductFile&)            = delete;
   TextProductFile& operator=(const TextProductFile&) = delete;

   TextProductFile(TextProductFile&&) noexcept;
   TextProductFile& operator=(TextProductFile&&) noexcept;

   size_t                                           message_count() const;
   std::vector<std::shared_ptr<TextProductMessage>> messages() const;
   std::shared_ptr<TextProductMessage>              message(size_t i) const;

   bool LoadFile(const std::string& filename);
   bool LoadData(std::istream& is);

private:
   std::unique_ptr<TextProductFileImpl> p;
};

} // namespace awips
} // namespace scwx
