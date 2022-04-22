#include <scwx/util/logger.hpp>

#include <aws/core/Aws.h>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

int main(int argc, char** argv)
{
   scwx::util::Logger::Initialize();
   spdlog::set_level(spdlog::level::debug);

   Aws::SDKOptions awsSdkOptions;
   Aws::InitAPI(awsSdkOptions);

   ::testing::InitGoogleTest(&argc, argv);
   int result = RUN_ALL_TESTS();

   Aws::ShutdownAPI(awsSdkOptions);

   return result;
}
