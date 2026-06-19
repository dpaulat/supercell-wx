#include <scwx/zip/zip_stream_writer.hpp>
#include <scwx/util/logger.hpp>

#include <cstdlib>
#include <fstream>

#include <zip.h>

namespace scwx::zip
{

static const std::string logPrefix_ = "scwx::zip::zip_stream_writer";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

// Stream state for writing to an ostream
struct ReadWriteStreamState
{
   std::iostream* stream;
   zip_int64_t    offset {0};
   zip_int64_t    length {-1};
   bool           error {false};

   ReadWriteStreamState(std::iostream* s) : stream {s}
   {
      if (stream)
      {
         stream->seekg(0, std::ios::end);
         length = stream->tellg();
         stream->seekg(0, std::ios::beg);
      }
   }
};

class ZipStreamWriter::Impl
{
public:
   explicit Impl()              = default;
   ~Impl()                      = default;
   Impl(const Impl&)            = delete;
   Impl& operator=(const Impl&) = delete;
   Impl(Impl&&)                 = delete;
   Impl& operator=(Impl&&)      = delete;

   void Initialize(std::iostream& stream);

   template<typename T>
   bool AddFile(const std::string& filename,
                const T&           content,
                zip_flags_t        flags = ZIP_FL_OVERWRITE);
   zip_t*
   OpenFromIostream(std::iostream& stream, int flags, zip_error_t* error);
   zip_int64_t Write(void* data, zip_uint64_t len, zip_source_cmd_t cmd);

   static zip_int64_t ReadWriteCallback(void*            userdata,
                                        void*            data,
                                        zip_uint64_t     len,
                                        zip_source_cmd_t cmd);

   zip_t*                                archive_ {nullptr};
   std::unique_ptr<ReadWriteStreamState> state_ {nullptr};

   std::fstream fstream_;
};

ZipStreamWriter::ZipStreamWriter(const std::string& filename) :
    p(std::make_unique<Impl>())
{
   p->fstream_ = std::fstream {filename,
                               std::ios_base::in | std::ios_base::out |
                                  std::ios_base::trunc | std::ios_base::binary};

   if (p->fstream_.is_open())
   {
      p->Initialize(p->fstream_);
   }
}

ZipStreamWriter::ZipStreamWriter(std::iostream& stream) :
    p(std::make_unique<Impl>())
{
   p->Initialize(stream);
}

ZipStreamWriter::~ZipStreamWriter()
{
   if (p->archive_)
   {
      zip_close(p->archive_);
   }
};

ZipStreamWriter::ZipStreamWriter(ZipStreamWriter&&) noexcept = default;
ZipStreamWriter&
ZipStreamWriter::operator=(ZipStreamWriter&&) noexcept = default;

void ZipStreamWriter::Impl::Initialize(std::iostream& stream)
{
   zip_error_t error;
   zip_error_init(&error);

   archive_ = OpenFromIostream(stream, ZIP_CREATE | ZIP_TRUNCATE, &error);

   if (!archive_)
   {
      logger_->error("Failed to create archive");
   }
}

bool ZipStreamWriter::IsOpen() const
{
   return p->archive_ != nullptr;
}

bool ZipStreamWriter::AddFile(const std::string& filename,
                              const std::string& content)
{
   return p->archive_ ? p->AddFile(filename, content) : false;
}

bool ZipStreamWriter::AddFile(const std::string&               filename,
                              const std::vector<std::uint8_t>& content)
{
   return p->archive_ ? p->AddFile(filename, content) : false;
}

void ZipStreamWriter::Close()
{
   if (p->archive_)
   {
      zip_close(p->archive_);
      p->archive_ = nullptr;
   }
}

// Callback function for writing to an ostream
zip_int64_t
ZipStreamWriter::Impl::Write(void* data, zip_uint64_t len, zip_source_cmd_t cmd)
{
   auto& rws = state_;

   switch (cmd)
   {
   case ZIP_SOURCE_OPEN:
      rws->stream->clear();
      rws->stream->seekg(0, std::ios::beg);
      rws->offset = 0;
      return 0;

   case ZIP_SOURCE_READ:
   {
      if (rws->error)
      {
         return -1;
      }

      rws->stream->read(static_cast<char*>(data),
                        static_cast<std::streamsize>(len));
      const zip_int64_t bytes_read = rws->stream->gcount();

      if (rws->stream->bad())
      {
         rws->error = true;
         return -1;
      }

      rws->offset += bytes_read;
      return bytes_read;
   }

   case ZIP_SOURCE_CLOSE:
      return 0;

   case ZIP_SOURCE_STAT:
   {
      auto* st = static_cast<zip_stat_t*>(data);
      zip_stat_init(st);
      st->valid = ZIP_STAT_SIZE;
      st->size  = rws->length;
      return sizeof(zip_stat_t);
   }

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
         new_offset = rws->offset + args->offset;
         break;
      case SEEK_END:
         new_offset = rws->length + args->offset;
         break;
      default:
         return -1;
      }

      if (new_offset < 0 || new_offset > rws->length)
      {
         return -1;
      }

      rws->stream->clear();
      rws->stream->seekg(new_offset, std::ios::beg);
      rws->offset = new_offset;
      return 0;
   }

   case ZIP_SOURCE_TELL:
      return rws->offset;

   case ZIP_SOURCE_BEGIN_WRITE:
      rws->stream->clear();
      rws->stream->seekp(0, std::ios::beg);
      rws->offset = 0;
      return 0;

   case ZIP_SOURCE_COMMIT_WRITE:
      rws->stream->flush();
      if (!rws->stream->good())
      {
         return -1;
      }
      // Update length after write
      rws->stream->seekp(0, std::ios::end);
      rws->length = rws->stream->tellp();
      return 0;

   case ZIP_SOURCE_ROLLBACK_WRITE:
      return 0;

   case ZIP_SOURCE_WRITE:
   {
      if (rws->error)
      {
         return -1;
      }

      rws->stream->write(static_cast<const char*>(data),
                         static_cast<std::streamsize>(len));

      if (!rws->stream->good())
      {
         rws->error = true;
         return -1;
      }

      rws->offset += static_cast<zip_int64_t>(len);
      return static_cast<zip_int64_t>(len);
   }

   case ZIP_SOURCE_SEEK_WRITE:
   {
      const auto* args       = static_cast<zip_source_args_seek_t*>(data);
      zip_int64_t new_offset = 0;

      switch (args->whence)
      {
      case SEEK_SET:
         new_offset = args->offset;
         break;
      case SEEK_CUR:
         new_offset = rws->offset + args->offset;
         break;
      case SEEK_END:
         rws->stream->seekp(0, std::ios::end);
         new_offset = rws->stream->tellp() + args->offset;
         break;
      default:
         return -1;
      }

      if (new_offset < 0)
      {
         return -1;
      }

      rws->stream->seekp(new_offset, std::ios::beg);
      rws->offset = new_offset;
      return 0;
   }

   case ZIP_SOURCE_TELL_WRITE:
      return rws->offset;

   case ZIP_SOURCE_REMOVE:
      return 0;

   case ZIP_SOURCE_FREE:
      rws.reset();
      return 0;

   case ZIP_SOURCE_ERROR:
   {
      auto* err = static_cast<zip_error_t*>(data);
      zip_error_init(err);
      if (rws->error)
      {
         zip_error_set(err, ZIP_ER_READ, 0);
      }
      return sizeof(zip_error_t);
   }

   case ZIP_SOURCE_SUPPORTS:
      return zip_source_make_command_bitmap(ZIP_SOURCE_OPEN,
                                            ZIP_SOURCE_READ,
                                            ZIP_SOURCE_CLOSE,
                                            ZIP_SOURCE_STAT,
                                            ZIP_SOURCE_SEEK,
                                            ZIP_SOURCE_TELL,
                                            ZIP_SOURCE_BEGIN_WRITE,
                                            ZIP_SOURCE_COMMIT_WRITE,
                                            ZIP_SOURCE_ROLLBACK_WRITE,
                                            ZIP_SOURCE_WRITE,
                                            ZIP_SOURCE_SEEK_WRITE,
                                            ZIP_SOURCE_TELL_WRITE,
                                            ZIP_SOURCE_REMOVE,
                                            ZIP_SOURCE_FREE,
                                            ZIP_SOURCE_ERROR,
                                            -1);

   default:
      return -1;
   }
}

// Create a zip archive from an output stream (for writing)
zip_t* ZipStreamWriter::Impl::OpenFromIostream(std::iostream& stream,
                                               int            flags,
                                               zip_error_t*   error)
{
   state_ = std::make_unique<ReadWriteStreamState>(&stream);
   zip_source_t* src =
      zip_source_function_create(Impl::ReadWriteCallback, this, error);

   if (!src)
   {
      state_.reset();
      return nullptr;
   }

   zip_t* archive = zip_open_from_source(src, flags, error);
   if (!archive)
   {
      zip_source_free(src);
      return nullptr;
   }

   return archive;
}

// Add a file to zip archive
template<typename T>
bool ZipStreamWriter::Impl::AddFile(const std::string& filename,
                                    const T&           content,
                                    zip_flags_t        flags)
{
   // NOLINTBEGIN(cppcoreguidelines-owning-memory)
   // NOLINTBEGIN(cppcoreguidelines-no-malloc)
   // NOLINTBEGIN(clang-analyzer-unix.Malloc)

   // Memory is managed by libzip
   void* buffer = std::malloc(content.size());

   // NOLINTNEXTLINE(bugprone-not-null-terminated-result): raw copy of data
   std::memcpy(buffer, content.data(), content.size());

   zip_source_t* src = zip_source_buffer(archive_, buffer, content.size(), 1);
   if (!src)
   {
      std::free(buffer);
      return false;
   }

   const zip_int64_t idx = zip_file_add(archive_, filename.c_str(), src, flags);
   if (idx < 0)
   {
      zip_source_free(src);
      return false;
   }

   // NOLINTEND(clang-analyzer-unix.Malloc)
   // NOLINTEND(cppcoreguidelines-no-malloc)
   // NOLINTEND(cppcoreguidelines-owning-memory)

   return true;
}

zip_int64_t ZipStreamWriter::Impl::ReadWriteCallback(void*            userdata,
                                                     void*            data,
                                                     zip_uint64_t     len,
                                                     zip_source_cmd_t cmd)
{

   auto* obj = static_cast<Impl*>(userdata);
   return obj->Write(data, len, cmd);
}

} // namespace scwx::zip
