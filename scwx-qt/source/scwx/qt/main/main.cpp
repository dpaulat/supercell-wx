#include <scwx/qt/config/radar_site.hpp>
#include <scwx/qt/main/main_window.hpp>
#include <scwx/qt/manager/resource_manager.hpp>
#include <scwx/qt/manager/settings_manager.hpp>
#include <scwx/util/logger.hpp>

#include <aws/core/Aws.h>
#include <spdlog/spdlog.h>
#include <QApplication>

int main(int argc, char* argv[])
{
   scwx::util::Logger::Initialize();
   spdlog::set_level(spdlog::level::debug);

   QCoreApplication::setApplicationName("Supercell Wx");

   Aws::SDKOptions awsSdkOptions;
   Aws::InitAPI(awsSdkOptions);

   scwx::qt::config::RadarSite::Initialize();
   scwx::qt::manager::SettingsManager::Initialize();
   scwx::qt::manager::ResourceManager::PreLoad();

   QApplication               a(argc, argv);
   scwx::qt::main::MainWindow w;
   w.show();
   int result = a.exec();

   Aws::ShutdownAPI(awsSdkOptions);

   return result;
}
