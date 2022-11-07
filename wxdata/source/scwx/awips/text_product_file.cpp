#include <scwx/awips/text_product_file.hpp>
#include <scwx/util/logger.hpp>

#include <fstream>

namespace scwx
{
namespace awips
{

static const std::string logPrefix_ = "scwx::awips::text_product_file";
static const auto        logger_    = util::Logger::Create(logPrefix_);

class TextProductFileImpl
{
public:
   explicit TextProductFileImpl() : messages_ {} {};
   ~TextProductFileImpl() = default;

   std::vector<std::shared_ptr<TextProductMessage>> messages_;
};

TextProductFile::TextProductFile() : p(std::make_unique<TextProductFileImpl>())
{
}
TextProductFile::~TextProductFile() = default;

TextProductFile::TextProductFile(TextProductFile&&) noexcept = default;
TextProductFile&
TextProductFile::operator=(TextProductFile&&) noexcept = default;

size_t TextProductFile::message_count() const
{
   return p->messages_.size();
}

std::vector<std::shared_ptr<TextProductMessage>>
TextProductFile::messages() const
{
   return p->messages_;
}

std::shared_ptr<TextProductMessage> TextProductFile::message(size_t i) const
{
   return p->messages_[i];
}

bool TextProductFile::LoadFile(const std::string& filename)
{
   logger_->debug("LoadFile: {}", filename);
   bool fileValid = true;

   std::ifstream f(filename, std::ios_base::in | std::ios_base::binary);
   if (!f.good())
   {
      logger_->warn("Could not open file for reading: {}", filename);
      fileValid = false;
   }

   if (fileValid)
   {
      fileValid = LoadData(f);
   }

   return fileValid;
}

bool TextProductFile::LoadData(std::istream& is)
{
   logger_->trace("Loading Data");

   while (!is.eof())
   {
      std::shared_ptr<TextProductMessage> message =
         TextProductMessage::Create(is);
      bool duplicate = false;

      if (message != nullptr)
      {
         for (auto m : p->messages_)
         {
            if (*m->wmo_header().get() == *message->wmo_header().get())
            {
               duplicate = true;
               break;
            }
         }

         if (!duplicate)
         {
            p->messages_.push_back(message);
         }
      }
      else
      {
         break;
      }
   }

   return !p->messages_.empty();
}

} // namespace awips
} // namespace scwx
