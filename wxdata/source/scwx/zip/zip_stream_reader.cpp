#include <scwx/zip/zip_stream_reader.hpp>
#include <scwx/util/logger.hpp>

#include <fstream>

#include <zip.h>

namespace scwx::zip
{

static const std::string logPrefix_ = "scwx::zip::zip_stream_reader";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

// Stream state for reading from an istream
struct ReadStreamState
{
   std::istream* stream {nullptr};
   zip_int64_t   offset {0};
   zip_int64_t   length {-1};
   bool          error {false};

   ReadStreamState(std::istream* s) : stream {s}
   {
      if (stream)
      {
         stream->seekg(0, std::ios::end);
         length = stream->tellg();
         stream->seekg(0, std::ios::beg);
      }
   }
};

class ZipStreamReader::Impl
{
public:
   explicit Impl()              = default;
   ~Impl()                      = default;
   Impl(const Impl&)            = delete;
   Impl& operator=(const Impl&) = delete;
   Impl(Impl&&)                 = delete;
   Impl& operator=(Impl&&)      = delete;

   void Initialize(std::istream& stream);

   zip_int64_t Read(void* data, zip_uint64_t len, zip_source_cmd_t cmd);
   template<typename T>
   bool          ReadFile(const std::string& filename, T& output);
   zip_source_t* SourceFromIstream(std::istream& stream);

   static zip_int64_t ReadCallback(void*            userdata,
                                   void*            data,
                                   zip_uint64_t     len,
                                   zip_source_cmd_t cmd);

   zip_t*                           archive_ {nullptr};
   std::unique_ptr<ReadStreamState> state_ {nullptr};

   std::ifstream fstream_;
};

ZipStreamReader::ZipStreamReader(const std::string& filename) :
    p(std::make_unique<Impl>())
{
   p->fstream_ = std::ifstream {filename, std::ios_base::binary};

   if (p->fstream_.is_open())
   {
      p->Initialize(p->fstream_);
   }
}

ZipStreamReader::ZipStreamReader(std::istream& stream) :
    p(std::make_unique<Impl>())
{
   p->Initialize(stream);
}

ZipStreamReader::~ZipStreamReader()
{
   if (p->archive_)
   {
      zip_close(p->archive_);
   }
};

ZipStreamReader::ZipStreamReader(ZipStreamReader&&) noexcept = default;
ZipStreamReader&
ZipStreamReader::operator=(ZipStreamReader&&) noexcept = default;

void ZipStreamReader::Impl::Initialize(std::istream& stream)
{
   zip_error_t error;
   zip_error_init(&error);

   zip_source_t* src = SourceFromIstream(stream);
   if (src)
   {
      archive_ = zip_open_from_source(src, ZIP_RDONLY, &error);
      if (!archive_)
      {
         logger_->error("Failed to open archive");
         zip_source_free(src);
         state_.reset();
      }
   }
}

bool ZipStreamReader::IsOpen() const
{
   return p->archive_ != nullptr;
}

std::vector<std::string> ZipStreamReader::ListFiles()
{
   std::vector<std::string> files {};

   if (p->archive_)
   {
      const zip_int64_t num_entries = zip_get_num_entries(p->archive_, 0);

      for (zip_int64_t i = 0; i < num_entries; i++)
      {
         const char* name = zip_get_name(p->archive_, i, 0);
         if (name)
         {
            files.emplace_back(name);
         }
      }
   }

   return files;
}

bool ZipStreamReader::ReadFile(const std::string& filename, std::string& output)
{
   return p->archive_ ? p->ReadFile(filename, output) : false;
}

bool ZipStreamReader::ReadFile(const std::string&         filename,
                               std::vector<std::uint8_t>& output)
{
   return p->archive_ ? p->ReadFile(filename, output) : false;
}

void ZipStreamReader::Close()
{
   if (p->archive_)
   {
      zip_close(p->archive_);
      p->archive_ = nullptr;
   }
}

// Callback function for reading from an istream
zip_int64_t
ZipStreamReader::Impl::Read(void* data, zip_uint64_t len, zip_source_cmd_t cmd)
{
   auto& rs = state_;

   switch (cmd)
   {
   case ZIP_SOURCE_OPEN:
      rs->stream->clear();
      rs->stream->seekg(0, std::ios::beg);
      rs->offset = 0;
      return 0;

   case ZIP_SOURCE_READ:
   {
      if (rs->error)
      {
         return -1;
      }

      rs->stream->read(static_cast<char*>(data),
                       static_cast<std::streamsize>(len));
      const zip_int64_t bytes_read = rs->stream->gcount();

      if (rs->stream->bad())
      {
         rs->error = true;
         return -1;
      }

      rs->offset += bytes_read;
      return bytes_read;
   }

   case ZIP_SOURCE_CLOSE:
      return 0;

   case ZIP_SOURCE_STAT:
   {
      auto* st = static_cast<zip_stat_t*>(data);
      zip_stat_init(st);
      st->valid = ZIP_STAT_SIZE;
      st->size  = rs->length;
      return sizeof(zip_stat_t);
   }

   case ZIP_SOURCE_ERROR:
   {
      auto* err = static_cast<zip_error_t*>(data);
      zip_error_init(err);
      if (rs->error)
      {
         zip_error_set(err, ZIP_ER_READ, 0);
      }
      return sizeof(zip_error_t);
   }

   case ZIP_SOURCE_FREE:
      rs.reset();
      return 0;

   case ZIP_SOURCE_SEEK:
   {
      auto*       args       = static_cast<zip_source_args_seek_t*>(data);
      zip_int64_t new_offset = 0;

      switch (args->whence)
      {
      case SEEK_SET:
         new_offset = args->offset;
         break;
      case SEEK_CUR:
         new_offset = rs->offset + args->offset;
         break;
      case SEEK_END:
         new_offset = rs->length + args->offset;
         break;
      default:
         return -1;
      }

      if (new_offset < 0 || new_offset > rs->length)
      {
         return -1;
      }

      rs->stream->clear();
      rs->stream->seekg(new_offset, std::ios::beg);
      rs->offset = new_offset;
      return 0;
   }

   case ZIP_SOURCE_TELL:
      return rs->offset;

   case ZIP_SOURCE_SUPPORTS:
      return zip_source_make_command_bitmap(ZIP_SOURCE_OPEN,
                                            ZIP_SOURCE_READ,
                                            ZIP_SOURCE_CLOSE,
                                            ZIP_SOURCE_STAT,
                                            ZIP_SOURCE_ERROR,
                                            ZIP_SOURCE_FREE,
                                            ZIP_SOURCE_SEEK,
                                            ZIP_SOURCE_TELL,
                                            -1);

   default:
      return -1;
   }
}

template<typename T>
bool ZipStreamReader::Impl::ReadFile(const std::string& filename, T& output)
{
   zip_stat_t st;
   if (zip_stat(archive_, filename.c_str(), 0, &st) != 0)
   {
      return false;
   }

   zip_file_t* file = zip_fopen(archive_, filename.c_str(), 0);
   if (!file)
   {
      return false;
   }

   output.resize(st.size);
   const zip_int64_t bytes_read = zip_fread(file, output.data(), st.size);
   zip_fclose(file);

   return bytes_read == static_cast<zip_int64_t>(st.size);
}

// Create a zip_source from an input stream (for reading)
zip_source_t* ZipStreamReader::Impl::SourceFromIstream(std::istream& stream)
{
   state_ = std::make_unique<ReadStreamState>(&stream);
   zip_source_t* src =
      zip_source_function_create(Impl::ReadCallback, this, nullptr);

   if (!src)
   {
      state_.reset();
      return nullptr;
   }

   return src;
}

zip_int64_t ZipStreamReader::Impl::ReadCallback(void*            userdata,
                                                void*            data,
                                                zip_uint64_t     len,
                                                zip_source_cmd_t cmd)
{

   auto* obj = static_cast<Impl*>(userdata);
   return obj->Read(data, len, cmd);
}

} // namespace scwx::zip
