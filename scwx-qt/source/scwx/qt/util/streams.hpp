#pragma once

#include <boost/iostreams/categories.hpp>
#include <QIODevice>

namespace scwx
{
namespace qt
{
namespace util
{

class IoDeviceSource
{
public:
   typedef char                         char_type;
   typedef boost::iostreams::source_tag category;

   IoDeviceSource(QIODevice& source) : source_ {source} {}
   ~IoDeviceSource() {}

   std::streamsize read(char* buffer, std::streamsize n)
   {
      return source_.read(buffer, n);
   }

private:
   QIODevice& source_;
};

} // namespace util
} // namespace qt
} // namespace scwx
