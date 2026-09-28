#include <scwx/util/grib_idx.hpp>

#include <fmt/format.h>

namespace scwx::util::grib_idx
{

namespace
{

std::vector<std::string> SplitLine(const std::string& line)
{
   std::vector<std::string> tokens;
   std::size_t              start = 0;

   while (true)
   {
      std::size_t pos = line.find(':', start);
      if (pos == std::string::npos)
      {
         tokens.push_back(line.substr(start));
         break;
      }
      tokens.push_back(line.substr(start, pos - start));
      start = pos + 1;
   }

   return tokens;
}

} // namespace

std::vector<IdxRecord> ParseIdx(const std::string& idxText)
{
   std::vector<IdxRecord> records;

   std::size_t lineStart = 0;
   while (lineStart <= idxText.size())
   {
      std::size_t lineEnd = idxText.find('\n', lineStart);
      std::string line    = (lineEnd == std::string::npos) ?
                               idxText.substr(lineStart) :
                               idxText.substr(lineStart, lineEnd - lineStart);

      if (!line.empty() && line.back() == '\r')
      {
         line.pop_back();
      }

      if (!line.empty())
      {
         auto tokens = SplitLine(line);

         // messageNumber, byteOffset, referenceTime, parameter, level,
         // step -- qualifier (tokens[6+]) is optional.
         if (tokens.size() >= 6)
         {
            try
            {
               IdxRecord record;
               record.messageNumber = std::stoi(tokens[0]);
               record.byteOffset    = std::stoll(tokens[1]);
               record.referenceTime = tokens[2];
               record.parameter     = tokens[3];
               record.level         = tokens[4];
               record.step          = tokens[5];

               if (tokens.size() > 6)
               {
                  std::string qualifier = tokens[6];
                  for (std::size_t i = 7; i < tokens.size(); ++i)
                  {
                     qualifier += ':';
                     qualifier += tokens[i];
                  }
                  record.qualifier = std::move(qualifier);
               }

               records.push_back(std::move(record));
            }
            catch (const std::exception&)
            {
               // Malformed numeric field -- skip this line rather than
               // failing the whole idx over one bad entry.
            }
         }
      }

      if (lineEnd == std::string::npos)
      {
         break;
      }
      lineStart = lineEnd + 1;
   }

   return records;
}

ByteRange RangeForRecord(const std::vector<IdxRecord>& records,
                         std::size_t                   index)
{
   ByteRange range;
   range.start = records.at(index).byteOffset;

   if (index + 1 < records.size())
   {
      range.end = records[index + 1].byteOffset - 1;
   }

   return range;
}

std::optional<std::size_t>
FindRecord(const std::vector<IdxRecord>& records,
          const std::string&            parameter,
          const std::string&            level,
          const std::string&            qualifier)
{
   for (std::size_t i = 0; i < records.size(); ++i)
   {
      if (records[i].parameter == parameter && records[i].level == level &&
          records[i].qualifier == qualifier)
      {
         return i;
      }
   }

   return std::nullopt;
}

std::string ToRangeHeader(const ByteRange& range)
{
   if (range.end.has_value())
   {
      return fmt::format("bytes={}-{}", range.start, *range.end);
   }

   return fmt::format("bytes={}-", range.start);
}

} // namespace scwx::util::grib_idx
