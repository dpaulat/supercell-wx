#include <scwx/wsr88d/nexrad_file.hpp>
#include <scwx/util/logger.hpp>

#include <fstream>

namespace scwx
{
namespace wsr88d
{

static const std::string logPrefix_ = "scwx::wsr88d::nexrad_file";
static const auto        logger_    = util::Logger::Create(logPrefix_);

class NexradFileImpl
{
public:
   explicit NexradFileImpl() {}
   ~NexradFileImpl() = default;

   std::vector<std::uint8_t> fileData_ {};
   std::string               filename_ {};
};

NexradFile::NexradFile() : p(std::make_unique<NexradFileImpl>()) {}
NexradFile::~NexradFile() = default;

NexradFile::NexradFile(NexradFile&&) noexcept            = default;
NexradFile& NexradFile::operator=(NexradFile&&) noexcept = default;

const std::vector<std::uint8_t>& NexradFile::file_data() const
{ return p->fileData_; }

const std::string& NexradFile::filename() const
{ return p->filename_; }

bool NexradFile::has_file_data() const
{ return !p->fileData_.empty(); }

bool NexradFile::is_gzip_compressed() const
{
   static constexpr std::uint8_t kGzipMagic0 = 0x1f;
   static constexpr std::uint8_t kGzipMagic1 = 0x8b;

   return p->fileData_.size() >= 2 && p->fileData_[0] == kGzipMagic0 &&
          p->fileData_[1] == kGzipMagic1;
}

void NexradFile::set_file_data(std::vector<std::uint8_t> data)
{ p->fileData_ = std::move(data); }

void NexradFile::set_filename(std::string filename)
{ p->filename_ = std::move(filename); }

void NexradFile::set_filename_from_path(const std::string& path)
{
   const std::size_t pos = path.find_last_of("/\\");
   p->filename_ = (pos == std::string::npos) ? path : path.substr(pos + 1);
}

bool NexradFile::SaveFile(const std::string& filename) const
{
   if (p->fileData_.empty())
   {
      logger_->warn(
         "Cannot save NEXRAD file, original data is not available: {}",
         filename);
      return false;
   }

   std::ofstream ofs(filename,
                     std::ios_base::out | std::ios_base::binary |
                        std::ios_base::trunc);
   if (!ofs.is_open() || !ofs.good())
   {
      logger_->error("Could not open file for writing: {}", filename);
      return false;
   }

   ofs.write(reinterpret_cast<const char*>(p->fileData_.data()),
             static_cast<std::streamsize>(p->fileData_.size()));

   if (!ofs.good())
   {
      logger_->error("Could not write NEXRAD file: {}", filename);
      return false;
   }

   logger_->info("Saved NEXRAD file: {}", filename);
   return true;
}

} // namespace wsr88d
} // namespace scwx
