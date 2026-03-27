#include <scwx/qt/main/program_options.hpp>
#include <scwx/util/logger.hpp>

#include <boost/program_options.hpp>
#include <fmt/ranges.h>

namespace scwx::qt::main::ProgramOptions
{

static const std::string logPrefix_ = "scwx::qt::main::program_options";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static Options programOptions_ {};

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
      const auto parsed =
         // boost::program_options::command_line_parser(argc, argv)
         boost::program_options::command_line_parser(
            static_cast<int>(args.size()), args.data())
            .options(visibleOptions)
            .allow_unregistered()
            .run();

      boost::program_options::store(parsed, vm);
      boost::program_options::notify(vm);

      boost::program_options::collect_unrecognized(
         parsed.options, boost::program_options::include_positional);
   }
   catch (const boost::program_options::error& ex)
   {
      logger_->error("Error parsing command line arguments: {}", ex.what());

      std::ostringstream oss;
      oss << visibleOptions;
      logger_->info(oss.str());
   }
}

void HandleArguments()
{
   if (programOptions_.showHelp_)
   {
      PrintHelp();
   }

   if (!programOptions_.unrecognizedArgs_.empty())
   {
      logger_->warn("Unrecognized command line arguments: {}",
                    fmt::join(programOptions_.unrecognizedArgs_, " "));
   }
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
