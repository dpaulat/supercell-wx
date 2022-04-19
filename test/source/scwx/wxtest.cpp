#include <scwx/util/logger.hpp>

#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

int main(int argc, char** argv)
{
   scwx::util::Logger::Initialize();
   spdlog::set_level(spdlog::level::debug);

   ::testing::InitGoogleTest(&argc, argv);
   return RUN_ALL_TESTS();
}
