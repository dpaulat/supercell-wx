#include <scwx/util/strings.hpp>

#include <boost/algorithm/string/trim.hpp>

namespace scwx
{
namespace util
{

std::vector<std::string> ParseTokens(const std::string&       s,
                                     std::vector<std::string> delimiters,
                                     std::size_t              pos)
{
   std::vector<std::string> tokens {};
   std::size_t              findPos {};

   // Iterate through each delimiter
   for (std::size_t i = 0; i < delimiters.size() && pos != std::string::npos;
        ++i)
   {
      // Skip leading spaces
      while (pos < s.size() && std::isspace(s[pos]))
      {
         ++pos;
      }

      if (pos < s.size() && s[pos] == '"')
      {
         // Do not search for a delimeter within a quoted string
         findPos = s.find('"', pos + 1);

         // Increment search start to one after quotation mark
         if (findPos != std::string::npos)
         {
            ++findPos;
         }
      }
      else
      {
         // Search starting at the current position
         findPos = pos;
      }

      // Search for delimiter
      std::size_t nextPos = s.find_first_of(delimiters[i], findPos);

      // If the delimiter was not found, stop processing tokens
      if (nextPos == std::string::npos)
      {
         break;
      }

      // Add the current substring as a token
      auto& newToken = tokens.emplace_back(s.substr(pos, nextPos - pos));
      boost::trim(newToken);

      // Increment nextPos until the next non-space character
      while (++nextPos < s.size() && std::isspace(s[nextPos])) {}

      // Store new position value
      pos = nextPos;
   }

   // Add the remainder of the string as a token
   if (pos < s.size())
   {
      auto& newToken = tokens.emplace_back(s.substr(pos));
      boost::trim(newToken);
   }

   return tokens;
}

std::string ToString(const std::vector<std::string>& v)
{
   std::string value {};

   for (const std::string& s : v)
   {
      if (!value.empty())
      {
         value += ", ";
      }

      value += s;
   }

   return value;
}

std::optional<float> TryParseFloat(const std::string& str)
{
   std::optional<float> value = std::nullopt;

   try
   {
      value = static_cast<float>(std::stof(str));
   }
   catch (const std::exception&)
   {
   }

   return value;
}

template<typename T>
std::optional<T> TryParseUnsignedLong(const std::string& str)
{
   std::optional<T> value = std::nullopt;

   try
   {
      value = static_cast<T>(std::stoul(str));
   }
   catch (const std::exception&)
   {
   }

   return value;
}

template std::optional<std::uint16_t>
TryParseUnsignedLong<std::uint16_t>(const std::string& str);

} // namespace util
} // namespace scwx
