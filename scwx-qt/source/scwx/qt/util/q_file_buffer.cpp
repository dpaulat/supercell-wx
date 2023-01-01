#include <scwx/qt/util/q_file_buffer.hpp>
#include <scwx/util/logger.hpp>

#include <QFile>

namespace scwx
{
namespace qt
{
namespace util
{

static const std::string logPrefix_ = "scwx::qt::util::q_file_buffer";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

// Adapted from Microsoft filebuf reference implementation
// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

class QFileBuffer::Impl
{
public:
   explicit Impl(QFileBuffer* self) : self_ {self} {};
   ~Impl() = default;

   void ResetPutback();
   void SetPutback();

   QFileBuffer* self_;
   QFile        file_ {};
   char_type    putbackChar_ {};
   char_type*   putbackEback_ {nullptr};
   char_type*   putbackEgptr_ {nullptr};
};

void QFileBuffer::Impl::ResetPutback()
{
   if (self_->eback() == &putbackChar_)
   {
      // Restore get buffer after putback
      self_->setg(putbackEback_, putbackEback_, putbackEgptr_);
   }
}

void QFileBuffer::Impl::SetPutback()
{
   if (self_->eback() != &putbackChar_)
   {
      // Save current get buffer
      putbackEback_ = self_->eback();
      putbackEgptr_ = self_->egptr();
   }
   self_->setg(&putbackChar_, &putbackChar_, &putbackChar_ + 1);
}

QFileBuffer::QFileBuffer() : std::streambuf(), p(std::make_unique<Impl>(this))
{
   // Initialize read/write pointers
   setg(0, 0, 0);
   setp(0, 0);
}

QFileBuffer::QFileBuffer(const std::string&      filename,
                         std::ios_base::openmode mode) :
    QFileBuffer()
{
   open(filename, mode);
}

QFileBuffer::~QFileBuffer() = default;

QFileBuffer::QFileBuffer(QFileBuffer&&) noexcept            = default;
QFileBuffer& QFileBuffer::operator=(QFileBuffer&&) noexcept = default;

bool QFileBuffer::is_open() const
{
   return p->file_.isOpen();
}

QFileBuffer* QFileBuffer::open(const std::string&      filename,
                               std::ios_base::openmode mode)
{
   // If the associated file is already open, return a null pointer right away
   if (is_open())
   {
      return nullptr;
   }

   // Validate supported modes
   if (mode & std::ios_base::out || mode & std::ios_base::app ||
       mode & std::ios_base::trunc)
   {
      logger_->error("open(): write mode not supported");
      return nullptr;
   }

   // Convert std iostream flags to Qt flags
   QIODeviceBase::OpenMode flags {};
   if (mode & std::ios_base::in)
   {
      flags |= QIODeviceBase::OpenModeFlag::ReadOnly;
   }
   if ((mode & std::ios_base::binary) != std::ios_base::binary)
   {
      flags |= QIODeviceBase::OpenModeFlag::Text;
   }

   // Set the filename and open the file
   p->file_.setFileName(QString::fromStdString(filename));
   bool isOpen = p->file_.open(flags);

   if (isOpen)
   {
      // Seek to end if requested
      if (mode & std::ios_base::ate)
      {
         // Seek to end
         p->file_.seek(p->file_.size());
      }

      // Initialize read/write pointers
      setg(0, 0, 0);
      setp(0, 0);
   }

   return isOpen ? this : nullptr;
}

QFileBuffer* QFileBuffer::close()
{
   // If the associated file is already closed, return a null pointer right away
   if (!p->file_.isOpen())
   {
      return nullptr;
   }

   // Close the file
   p->file_.close();

   return this;
}

QFileBuffer::int_type QFileBuffer::pbackfail(int_type c)
{
   if (gptr() && eback() < gptr() &&
       (traits_type::eq_int_type(traits_type::eof(), c) ||
        traits_type::eq_int_type(traits_type::to_int_type(gptr()[-1]), c)))
   {
      // Just back up position
      gbump(static_cast<int>(sizeof(char_type)) * -1);
      return traits_type::not_eof(c);
   }
   else if (!is_open() || traits_type::eq_int_type(traits_type::eof(), c))
   {
      // No open QFile or EOF, fail
      return traits_type::eof();
   }
   else if (gptr() != &p->putbackChar_)
   {
      // Put back to buffer
      p->putbackChar_ = traits_type::to_char_type(c);
      p->SetPutback();
      return c;
   }
   else
   {
      // Nowhere to put back
      return traits_type::eof();
   }
}

QFileBuffer::pos_type QFileBuffer::seekoff(off_type               off,
                                           std::ios_base::seekdir dir,
                                           std::ios_base::openmode /* which */)
{
   pos_type newPos {pos_type(off_type(-1))};

   switch (dir)
   {
   case std::ios_base::beg:
      // Seek using absolute position
      newPos = seekpos(off, std::ios_base::in);
      break;

   case std::ios_base::cur:
   {
      const pos_type currentPos {p->file_.pos() - (egptr() - gptr())};
      pos_type       updatePos {currentPos + off};

      // If the putback buffer is not empty, decrement the offset
      if (gptr() == &p->putbackChar_)
      {
         updatePos -= static_cast<off_type>(sizeof(char_type));
      }

      // Seek the file
      if (p->file_.seek(updatePos))
      {
         // Record updated position
         newPos = updatePos;
      }

      break;
   }

   case std::ios_base::end:
   {
      const pos_type endPos {p->file_.size()};
      const pos_type updatePos {endPos + off};

      // Seek the file
      if (p->file_.seek(updatePos))
      {
         // Record updated position
         newPos = updatePos;
      }

      break;
   }
   }

   if (newPos != static_cast<off_type>(-1))
   {
      p->ResetPutback();
   }

   return newPos;
}

QFileBuffer::pos_type QFileBuffer::seekpos(pos_type pos,
                                           std::ios_base::openmode /* which */)
{
   pos_type newPos {pos_type(off_type(-1))};

   // Seek the file
   if (p->file_.seek(pos))
   {
      // Record updated position
      newPos = pos;

      p->ResetPutback();
   }

   return newPos;
}

QFileBuffer::int_type QFileBuffer::underflow()
{
   // This function is only called if gptr() == nullptr or gptr() > egptr()
   // (i.e., all buffer data has been read)
   int_type c;

   if (gptr() && gptr() < egptr())
   {
      // Return buffered
      return traits_type::to_int_type(*gptr());
   }
   else if (traits_type::eq_int_type(traits_type::eof(), c = uflow()))
   {
      // uflow failed, return EOF
      return c;
   }
   else
   {
      // Get a character, don't point past it
      pbackfail(c);
      return c;
   }
}

QFileBuffer::int_type QFileBuffer::uflow()
{
   if (gptr() && gptr() < egptr())
   {
      // Return buffered
      int_type c = traits_type::to_int_type(*gptr());
      gbump(sizeof(char_type));
      return c;
   }

   if (!p->file_.isOpen())
   {
      // No open QFile, fail
      return traits_type::eof();
   }

   p->ResetPutback();

   // Read the next character
   char_type c;
   return p->file_.read(reinterpret_cast<char*>(&c), sizeof(char_type)) > 0 ?
             traits_type::to_int_type(c) :
             traits_type::eof();
}

std::streamsize QFileBuffer::xsgetn(char_type* s, std::streamsize count)
{
   // Read up to count bytes, forwarding the return value from QFile::read
   // (return negative values as zero)
   return std::max<std::streamsize>(
      p->file_.read(reinterpret_cast<char*>(s), count * sizeof(char_type)), 0);
}

} // namespace util
} // namespace qt
} // namespace scwx
