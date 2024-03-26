#include <scwx/util/digest.hpp>
#include <scwx/util/logger.hpp>

namespace scwx
{
namespace util
{

static const std::string logPrefix_ = "scwx::util::digest";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

bool ComputeDigest(const EVP_MD*              mdtype,
                   std::istream&              is,
                   std::vector<std::uint8_t>& digest)
{
   int         mdsize;
   EVP_MD_CTX* mdctx = nullptr;

   digest.clear();

   if ((mdsize = EVP_MD_get_size(mdtype)) < 1)
   {
      logger_->error("Invalid digest");
      return false;
   }

   if ((mdctx = EVP_MD_CTX_new()) == nullptr)
   {
      logger_->error("Error allocating a digest context");
      return false;
   }

   if (!EVP_DigestInit_ex(mdctx, mdtype, nullptr))
   {
      logger_->error("Message digest initialization failed");
      EVP_MD_CTX_free(mdctx);
      return false;
   }

   is.seekg(0, std::ios_base::end);
   const std::size_t streamSize = is.tellg();
   is.seekg(0, std::ios_base::beg);

   std::size_t bytesRead = 0;
   std::size_t chunkSize = 4096;
   std::string fileData;
   fileData.resize(chunkSize);

   while (bytesRead < streamSize)
   {
      const std::size_t bytesRemaining = streamSize - bytesRead;
      const std::size_t readSize       = std::min(chunkSize, bytesRemaining);

      is.read(fileData.data(), readSize);

      if (!is.good() || !EVP_DigestUpdate(mdctx, fileData.data(), readSize))
      {
         logger_->error("Message digest update failed");
         EVP_MD_CTX_free(mdctx);
         return false;
      }

      bytesRead += readSize;
   }

   digest.resize(mdsize);

   if (!EVP_DigestFinal_ex(mdctx, digest.data(), nullptr))
   {
      logger_->error("Message digest finalization failed");
      EVP_MD_CTX_free(mdctx);
      digest.clear();
      return false;
   }

   EVP_MD_CTX_free(mdctx);

   return true;
}

} // namespace util
} // namespace scwx
