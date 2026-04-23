#include <scwx/qt/util/network.hpp>

#include <gtest/gtest.h>

namespace scwx
{
namespace qt
{
namespace util
{

const std::vector<std::pair<const std::string, const std::string>> testUrls = {
   {" https://example.com/path/to+a+test/file.txt ",
    "https://example.com/path/to+a+test/file.txt"},
   {"\thttps://example.com/path/to+a+test/file.txt\t",
    "https://example.com/path/to+a+test/file.txt"},
   {"\nhttps://example.com/path/to+a+test/file.txt\n",
    "https://example.com/path/to+a+test/file.txt"},
   {"\rhttps://example.com/path/to+a+test/file.txt\r",
    "https://example.com/path/to+a+test/file.txt"},
   {"\r\nhttps://example.com/path/to+a+test/file.txt\r\n",
    "https://example.com/path/to+a+test/file.txt"},
   {"    https://example.com/path/to+a+test/file.txt   ",
    "https://example.com/path/to+a+test/file.txt"},
   {"    \nhttps://example.com/path/to+a+test/file.txt  \n ",
    "https://example.com/path/to+a+test/file.txt"},

// Only tested for this OS because NormalizeUrl uses native separators
#ifdef _WIN32
   {" C:\\path\\to a test\\file.txt ", "C:\\path\\to a test\\file.txt"},
   {"\tC:\\path\\to a test\\file.txt\t", "C:\\path\\to a test\\file.txt"},
   {"\nC:\\path\\to a test\\file.txt\n", "C:\\path\\to a test\\file.txt"},
   {"\rC:\\path\\to a test\\file.txt\r", "C:\\path\\to a test\\file.txt"},
   {"\r\nC:\\path\\to a test\\file.txt\r\n", "C:\\path\\to a test\\file.txt"},
   {"    C:\\path\\to a test\\file.txt   ", "C:\\path\\to a test\\file.txt"},
   {"    \nC:\\path\\to a test\\file.txt  \n ",
    "C:\\path\\to a test\\file.txt"},

   {" C:/path/to a test/file.txt ", "C:\\path\\to a test\\file.txt"},
   {"\tC:/path/to a test/file.txt\t", "C:\\path\\to a test\\file.txt"},
   {"\nC:/path/to a test/file.txt\n", "C:\\path\\to a test\\file.txt"},
   {"\rC:/path/to a test/file.txt\r", "C:\\path\\to a test\\file.txt"},
   {"\r\nC:/path/to a test/file.txt\r\n", "C:\\path\\to a test\\file.txt"},
   {"    C:/path/to a test/file.txt   ", "C:\\path\\to a test\\file.txt"},
   {"    \nC:/path/to a test/file.txt  \n ", "C:\\path\\to a test\\file.txt"},
#else

   {" /path/to a test/file.txt ", "/path/to a test/file.txt"},
   {"\t/path/to a test/file.txt\t", "/path/to a test/file.txt"},
   {"\n/path/to a test/file.txt\n", "/path/to a test/file.txt"},
   {"\r/path/to a test/file.txt\r", "/path/to a test/file.txt"},
   {"\r\n/path/to a test/file.txt\r\n", "/path/to a test/file.txt"},
   {"    /path/to a test/file.txt   ", "/path/to a test/file.txt"},
   {"    \n/path/to a test/file.txt  \n ", "/path/to a test/file.txt"},
#endif
};

TEST(network, NormalizeUrl)
{
   for (auto& pair : testUrls)
   {
      const std::string& preNormalized = pair.first;
      const std::string& expNormalized = pair.second;

      std::string normalized = network::NormalizeUrl(preNormalized);
      EXPECT_EQ(normalized, expNormalized);
   }
}

} // namespace util
} // namespace qt
} // namespace scwx
