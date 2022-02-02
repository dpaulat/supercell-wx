#include <scwx/qt/config/radar_site.hpp>
#include <scwx/qt/main/main_window.hpp>
#include <scwx/qt/manager/resource_manager.hpp>
#include <scwx/qt/manager/settings_manager.hpp>

#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include <QApplication>

int main(int argc, char* argv[])
{
   boost::log::core::get()->set_filter(boost::log::trivial::severity >=
                                       boost::log::trivial::debug);

   QCoreApplication::setApplicationName("Supercell Wx");

   scwx::qt::config::RadarSite::Initialize();
   scwx::qt::manager::SettingsManager::Initialize();
   scwx::qt::manager::ResourceManager::PreLoad();

   QApplication               a(argc, argv);
   scwx::qt::main::MainWindow w;
   w.show();
   return a.exec();
}
