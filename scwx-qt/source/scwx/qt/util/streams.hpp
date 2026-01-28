#pragma once

#include <boost/iostreams/categories.hpp>
#include <boost/iostreams/positioning.hpp>
#include <QIODevice>

namespace scwx::qt::util
{

class IoDeviceSource
{
public:
   using char_type = char;
   using category  = boost::iostreams::seekable_device_tag;

   IoDeviceSource(QIODevice& source) : source_ {&source} {}
   ~IoDeviceSource()                                = default;
   IoDeviceSource(const IoDeviceSource&)            = default;
   IoDeviceSource& operator=(const IoDeviceSource&) = default;
   IoDeviceSource(IoDeviceSource&&)                 = default;
   IoDeviceSource& operator=(IoDeviceSource&&)      = default;

   std::streamsize read(char* buffer, std::streamsize n)
   {
      return source_->read(buffer, n);
   }

   std::streamsize write(const char* buffer, std::streamsize n)
   {
      return source_->write(buffer, n);
   }

   boost::iostreams::stream_offset seek(boost::iostreams::stream_offset off,
                                        std::ios_base::seekdir          way)
   {
      qint64 newPos = 0;

      switch (way)
      {
      case std::ios_base::beg:
         newPos = off;
         break;
      case std::ios_base::cur:
         newPos = source_->pos() + off;
         break;
      case std::ios_base::end:
         newPos = source_->size() + off;
         break;
      default:
         return -1;
      }

      if (source_->seek(newPos))
      {
         return newPos;
      }
      else
      {
         return -1;
      }
   }

private:
   QIODevice* source_;
};

} // namespace scwx::qt::util
