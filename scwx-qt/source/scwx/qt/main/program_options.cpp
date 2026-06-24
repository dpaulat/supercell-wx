#include <scwx/qt/main/program_options.hpp>
#include <scwx/util/logger.hpp>

#include <sstream>

#include <boost/program_options.hpp>
#include <fmt/ranges.h>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace scwx::qt::main::ProgramOptions
{

static const std::string logPrefix_ = "scwx::qt::main::program_options";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static Options     programOptions_ {};
static std::string errorMessage_ {};

static void                                               EnableConsole();
static const boost::program_options::options_description& GetVisibleOptions();
static void                                               PrintHelp();

const Options& GetOptions()
{
   return programOptions_;
}

void ParseArguments(std::span<const char* const> args)
{
   const boost::program_options::options_description& visibleOptions =
      GetVisibleOptions();

   boost::program_options::variables_map vm;
   try
   {
      const auto parsed = boost::program_options::command_line_parser(
                             static_cast<int>(args.size()), args.data())
                             .options(visibleOptions)
                             .allow_unregistered()
                             .run();

      boost::program_options::store(parsed, vm);
      boost::program_options::notify(vm);

      programOptions_.unrecognizedArgs_ =
         boost::program_options::collect_unrecognized(
            parsed.options, boost::program_options::include_positional);

      if (programOptions_.enableConsole_ || programOptions_.showHelp_)
      {
         EnableConsole();
      }
   }
   catch (const boost::program_options::error& ex)
   {
      EnableConsole();
      programOptions_.showHelp_ = true;
      errorMessage_ =
         fmt::format("Error parsing command line arguments: {}", ex.what());
   }
}

void HandleArguments(bool& exit)
{
   if (programOptions_.showHelp_)
   {
      if (!errorMessage_.empty())
      {
         logger_->error(errorMessage_);
      }

      PrintHelp();
      exit = true;
   }

   if (!programOptions_.unrecognizedArgs_.empty())
   {
      logger_->warn("Unrecognized command line arguments: {}",
                    fmt::join(programOptions_.unrecognizedArgs_, " "));
   }
}

void Reset()
{
   programOptions_ = Options {};
}

void EnableConsole()
{
#if defined(_WIN32)
   // On Windows, enable console output by attaching to the parent process's
   // console (if it exists) or allocating a new console if not.
   if (AttachConsole(ATTACH_PARENT_PROCESS) || AllocConsole())
   {
      FILE* fp = nullptr;
      freopen_s(&fp, "CONOUT$", "w", stdout);
      freopen_s(&fp, "CONOUT$", "w", stderr);
      freopen_s(&fp, "CONIN$", "r", stdin);
   }
#endif
}

const boost::program_options::options_description& GetVisibleOptions()
{
   static bool                                        ftt = true;
   static boost::program_options::options_description visibleOptions_(
      "Options");

   if (ftt)
   {
      visibleOptions_.add_options() //
         ("help,h",
          boost::program_options::bool_switch(&programOptions_.showHelp_),
          "Display this help message") //
#if defined(_WIN32)
         ("console,c",
          boost::program_options::bool_switch(&programOptions_.enableConsole_),
          "Enable console output") //
#endif
         ("portable,p",
          boost::program_options::bool_switch(&programOptions_.portableMode_),
          "Run in portable mode, storing settings in the application "
          "directory") //
         ("settings-directory",
          boost::program_options::value<std::string>(
             &programOptions_.settingsDirectory_)
             ->value_name("path"),
          "Override the default settings directory with the specified path");

      ftt = false;
   }

   return visibleOptions_;
}

void PrintHelp()
{
   // clang-format off
      std::ostringstream oss;
      oss << "Supercell Wx - Free and open source advanced weather radar\n"
             "\n"
             "Usage:\n"
             "  supercell-wx [qt options] [options]\n"
             "\n"
          << GetVisibleOptions()
          << "\n"
             "Qt Options:\n"
             "  Qt platform and UI options (e.g., -style, -platform, -geometry)\n"
             "  may also be specified. These are consumed by the Qt framework\n"
             "  before application argument parsing and are not listed here.\n"
             "  See https://doc.qt.io/qt-6/qapplication.html for details.\n";
   // clang-format on

   logger_->info(oss.str());
}

} // namespace scwx::qt::main::ProgramOptions
