#pragma once

#include <istream>
#include <memory>
#include <string>

namespace scwx
{
namespace qt
{
namespace util
{

std::unique_ptr<std::istream>
OpenFile(const std::string&      filename,
         std::ios_base::openmode mode = std::ios_base::in);

} // namespace util
} // namespace qt
} // namespace scwx
