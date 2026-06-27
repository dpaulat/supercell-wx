// NOLINTNEXTLINE(bugprone-reserved-identifier) — MSVC-required macro name
#define _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING

#include <scwx/qt/config/county_database.hpp>
#include <scwx/qt/config/radar_site.hpp>
#include <scwx/qt/main/application_paths.hpp>
#include <scwx/qt/main/main_window.hpp>
#include <scwx/qt/main/process_validation.hpp>
#include <scwx/qt/main/program_options.hpp>
#include <scwx/qt/main/theme.hpp>
#include <scwx/qt/main/versions.hpp>
#include <scwx/qt/manager/log_manager.hpp>
#include <scwx/qt/manager/radar_product_manager.hpp>
#include <scwx/qt/manager/resource_manager.hpp>
#include <scwx/qt/manager/settings_manager.hpp>
#include <scwx/qt/manager/task_manager.hpp>
#include <scwx/qt/manager/thread_manager.hpp>
#include <scwx/qt/settings/general_settings.hpp>
#include <scwx/qt/types/qt_types.hpp>
#include <scwx/qt/ui/setup/setup_wizard.hpp>
#include <scwx/qt/main/check_privilege.hpp>
#include <scwx/common/application_state.hpp>
#include <scwx/network/cpr.hpp>
#include <scwx/util/environment.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/threads.hpp>

#include <string>
#include <vector>

#include <aws/core/Aws.h>
#include <boost/asio.hpp>
#include <fmt/format.h>
#include <QApplication>
#include <QLibraryInfo>
#include <QStandardPaths>
#include <QStyleHints>
#include <QSurfaceFormat>
#include <QTranslator>
#include <QSysInfo>

static const std::string logPrefix_ = "scwx::main";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static void InitializeOpenGL();

// NOLINTNEXTLINE(bugprone-exception-escape) — top-level entry; Qt/AWS paths may
// throw
int main(int argc, char* argv[])
{
   // Store arguments
   std::vector<std::string> args {};
   args.reserve(argc);
   for (int i = 0; i < argc; ++i)
   {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      args.emplace_back(argv[i]);
   }

   if (!scwx::util::GetEnvironment("SCWX_TEST").empty())
   {
      QStandardPaths::setTestModeEnabled(true);
   }

   QCoreApplication::setApplicationName("Supercell Wx");
   InitializeOpenGL();
   const QApplication a(argc, argv);

   scwx::qt::main::ProgramOptions::ParseArguments(
      {argv, static_cast<std::size_t>(argc)});

   // Initialize logger and application paths
   auto& logManager = scwx::qt::manager::LogManager::Instance();
   logManager.Initialize();
   scwx::qt::main::ApplicationPaths::Initialize();
   logManager.InitializeLogFile();

   const std::string osName = QSysInfo::prettyProductName().toStdString();
   logger_->info("Supercell Wx v{}.{} ({})",
                 scwx::qt::main::kVersionString_,
                 scwx::qt::main::kBuildNumber_,
                 scwx::qt::main::kCommitString_);
   logger_->info("Qt version {}",
                 QLibraryInfo::version().toString().toStdString());
   logger_->info("Running on {}", osName);

   bool exit = false;
   scwx::qt::main::ProgramOptions::HandleArguments(exit);
   if (exit)
   {
      return 0;
   }

   scwx::qt::main::ApplicationPaths::LogErrors();

   scwx::network::cpr::SetUserAgent(fmt::format(
      "SupercellWx/{} ({})", scwx::qt::main::kVersionString_, osName));

   // Enable internationalization support
   QTranslator translator;
   if (translator.load(QLocale(), "scwx", "_", ":/i18n"))
   {
      QCoreApplication::installTranslator(&translator);
   }

   // Test to see if scwx was run with high privilege
   scwx::qt::main::PrivilegeChecker privilegeChecker;
   if (privilegeChecker.pre_settings_check())
   {
      return 0;
   }

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
   const Aws::SDKOptions awsSdkOptions {};
   Aws::InitAPI(awsSdkOptions);

   // Initialize application
   scwx::qt::config::RadarSite::Initialize();
   scwx::qt::config::CountyDatabase::Initialize();
   scwx::qt::manager::TaskManager::Initialize();
   scwx::qt::manager::SettingsManager::Instance().Initialize();
   scwx::qt::manager::ResourceManager::Initialize();

   // Theme
   scwx::qt::main::ConfigureThemeForStartup(args);

   // Check process modules for compatibility
   scwx::qt::main::CheckProcessModules();

   int result = 0;
   if (privilegeChecker.post_settings_check())
   {
      result = 1;
   }
   else
   {
      // Run initial setup if required
      if (scwx::qt::ui::setup::SetupWizard::IsSetupRequired())
      {
         scwx::qt::ui::setup::SetupWizard w;
         w.show();
         a.exec();
      }

      // Run Qt main loop
      {
         scwx::qt::main::MainWindow w;

         bool initialized = false;

         try
         {
            w.show();
            initialized = true;
         }
         catch (const std::exception& ex)
         {
            logger_->critical(ex.what());
         }

         if (initialized)
         {
            result = a.exec();
         }
      }
   }

   // Deinitialize application
   scwx::qt::manager::RadarProductManager::Cleanup();

   // Stop Qt Threads
   scwx::qt::manager::ThreadManager::Instance().StopThreads();

   // Gracefully stop the io_context main loop
   work.reset();
   threadPool.join();

   // Shutdown application
   scwx::common::ApplicationState::Shutdown();
   scwx::qt::manager::ResourceManager::Shutdown();
   scwx::qt::manager::SettingsManager::Instance().Shutdown();
   scwx::qt::manager::TaskManager::Shutdown();

   // Shutdown AWS SDK
   Aws::ShutdownAPI(awsSdkOptions);

   return result;
}

static void InitializeOpenGL()
{
   QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts, true);

   QSurfaceFormat surfaceFormat = QSurfaceFormat::defaultFormat();
   surfaceFormat.setProfile(QSurfaceFormat::OpenGLContextProfile::CoreProfile);
   surfaceFormat.setRenderableType(QSurfaceFormat::RenderableType::OpenGL);

#if defined(__APPLE__)
   // For macOS, we must choose between OpenGL 4.1 Core and OpenGL 2.1
   // Compatibility. OpenGL 2.1 does not meet requirements for shaders used by
   // Supercell Wx.
   surfaceFormat.setVersion(4, 1);
#endif

   QSurfaceFormat::setDefaultFormat(surfaceFormat);
}
