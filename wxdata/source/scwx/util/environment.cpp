#include <scwx/util/environment.hpp>
#include <scwx/util/logger.hpp>

#ifndef _WIN32
#   include <cstdlib>
#endif

namespace scwx
{
namespace util
{

static const std::string logPrefix_ {"scwx::util::environment"};
static const auto        logger_ = util::Logger::Create(logPrefix_);

void SetEnvironment(const std::string& name, const std::string& value)
{
#ifdef _WIN32
   errno_t error = _putenv_s(name.c_str(), value.c_str());
#else
   int error = setenv(name.c_str(), value.c_str(), 1);
#endif

   if (error != 0)
   {
      logger_->warn("Could not set environment variable: {}={}", name, value);
   }
}

} // namespace util
} // namespace scwx
