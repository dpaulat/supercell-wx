#include <scwx/types/ondas_types.hpp>
#include <scwx/util/streams.hpp>

#include <cctype>
#include <sstream>

#include <boost/algorithm/string/trim.hpp>

namespace scwx::types::ondas
{

std::vector<OndasDirListRecord> ParseOndasDirList(const std::string& content)
{
   std::vector<OndasDirListRecord> records {};

   std::istringstream iss {content};
   std::string        line {};

   while (util::getline(iss, line))
   {
      boost::trim(line);
      if (line.empty())
      {
         continue;
      }

      OndasDirListRecord record {};

      // Try to parse as "size filename" first
      std::istringstream lineStream(line);
      std::size_t        size {0};

      const auto sizeParsed = (lineStream >> size) ? true : false;
      const auto nextChar   = lineStream.peek();

      // If the size was parsed and the next character is a space, then the line
      // is in the format "size filename". Otherwise, the entire line (including
      // any digits parsed) is the filename.
      if (sizeParsed && std::isspace(nextChar))
      {
         // Successfully read a size, now get the rest as filename
         // Skip any whitespace after the size
         lineStream >> std::ws;

         // Get the rest of the line as filename
         util::getline(lineStream, record.filename_);

         if (!record.filename_.empty())
         {
            // Format: "size filename"
            record.size_ = size;
         }
         else
         {
            // Size was read but no filename - treat whole line as filename
            record.filename_ = line;
            record.size_     = 0;
         }
      }
      else
      {
         // Format: "filename" only (couldn't parse as size)
         record.filename_ = line;
         record.size_     = 0;
      }

      records.push_back(std::move(record));
   }

   return records;
}

} // namespace scwx::types::ondas
