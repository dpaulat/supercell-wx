#include <scwx/qt/main/application_paths.hpp>
#include <scwx/util/logger.hpp>

#include <aws/core/Aws.h>
#include <gtest/gtest.h>
#include <QCoreApplication>
#include <spdlog/spdlog.h>

int main(int argc, char** argv)
{
   QCoreApplication app(argc, argv);

   scwx::util::Logger::Initialize();
   spdlog::set_level(spdlog::level::debug);

   scwx::qt::main::ApplicationPaths::Initialize();
   scwx::qt::main::ApplicationPaths::LogErrors();

   Aws::SDKOptions awsSdkOptions;
   Aws::InitAPI(awsSdkOptions);

   ::testing::InitGoogleTest(&argc, argv);
   int result = RUN_ALL_TESTS();

   Aws::ShutdownAPI(awsSdkOptions);

   return result;
}
