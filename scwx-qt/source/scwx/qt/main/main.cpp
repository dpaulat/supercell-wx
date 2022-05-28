#include <scwx/qt/config/radar_site.hpp>
#include <scwx/qt/main/main_window.hpp>
#include <scwx/qt/manager/radar_product_manager.hpp>
#include <scwx/qt/manager/resource_manager.hpp>
#include <scwx/qt/manager/settings_manager.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/threads.hpp>

#include <aws/core/Aws.h>
#include <boost/asio.hpp>
#include <spdlog/spdlog.h>
#include <QApplication>

static const std::string logPrefix_ = "scwx::main";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

int main(int argc, char* argv[])
{
   // Initialize logger
   scwx::util::Logger::Initialize();
   spdlog::set_level(spdlog::level::debug);

   QCoreApplication::setApplicationName("Supercell Wx");

   // Start the io_context main loop
   boost::asio::io_context& ioContext = scwx::util::io_context();
   auto                     work      = boost::asio::make_work_guard(ioContext);
   boost::asio::thread_pool threadPool {4};
   boost::asio::post(threadPool,
                     [&]()
                     {
                        while (true)
                        {
                           try
                           {
                              ioContext.run();
                              break; // run() exited normally
                           }
                           catch (std::exception& ex)
                           {
                              // Log exception and continue
                              logger_->error(ex.what());
                           }
                        }
                     });

   // Initialize AWS SDK
   Aws::SDKOptions awsSdkOptions;
   Aws::InitAPI(awsSdkOptions);

   // Initialize application
   scwx::qt::config::RadarSite::Initialize();
   scwx::qt::manager::SettingsManager::Initialize();
   scwx::qt::manager::ResourceManager::PreLoad();

   // Run Qt main loop
   int result;
   {
      QApplication               a(argc, argv);
      scwx::qt::main::MainWindow w;
      w.show();
      result = a.exec();
   }

   // Deinitialize application
   scwx::qt::manager::RadarProductManager::Cleanup();

   // Gracefully stop the io_context main loop
   work.reset();
   threadPool.join();

   // Shutdown AWS SDK
   Aws::ShutdownAPI(awsSdkOptions);

   return result;
}
