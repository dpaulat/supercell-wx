#include <scwx/qt/util/file.hpp>
#include <scwx/qt/util/q_file_input_stream.hpp>

#include <fstream>

#include <QFile>

namespace scwx
{
namespace qt
{
namespace util
{

static const std::string logPrefix_ = "scwx::qt::util::file";

std::unique_ptr<std::istream> OpenFile(const std::string&      filename,
                                       std::ios_base::openmode mode)
{
   if (filename.starts_with(':'))
   {
      return std::make_unique<QFileInputStream>(filename, mode);
   }
   else
   {
      return std::make_unique<std::ifstream>(filename, mode);
   }
}

} // namespace util
} // namespace qt
} // namespace scwx
