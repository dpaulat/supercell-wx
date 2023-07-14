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

std::string GetEnvironment(const std::string& name)
{
   std::string value {};

#ifdef _WIN32
   std::size_t       requiredSize;
   std::vector<char> data {};

   // Determine environment variable size
   getenv_s(&requiredSize, nullptr, 0, name.c_str());
   if (requiredSize == 0)
   {
      // Environment variable is not set
      return value;
   }

   // Request environment variable
   data.resize(requiredSize);
   getenv_s(&requiredSize, data.data(), requiredSize, name.c_str());

   // Store environment variable
   value = data.data();
#else
   const char* data = getenv(name.c_str());

   if (data != nullptr)
   {
      value = data;
   }
#endif

   return value;
}

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
