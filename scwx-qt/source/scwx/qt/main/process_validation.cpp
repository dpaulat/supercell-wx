#include <scwx/qt/main/process_validation.hpp>
#include <scwx/util/logger.hpp>

#if defined(_WIN32)
#   include <scwx/qt/settings/general_settings.hpp>

#   include <wtypes.h>
#   include <Psapi.h>

#   include <boost/algorithm/string/predicate.hpp>
#   include <boost/locale.hpp>
#   include <fmt/ranges.h>
#   include <QCheckBox>
#   include <QMessageBox>
#endif

namespace scwx::qt::main
{

static const std::string logPrefix_ = "scwx::qt::main::process_validation";
static const auto        logger_    = util::Logger::Create(logPrefix_);

void CheckProcessModules()
{
#if defined(_WIN32)
   HANDLE  process = GetCurrentProcess();
   HMODULE modules[1024];
   DWORD   cbNeeded = 0;

   std::vector<std::string> incompatibleDlls {};
   std::vector<std::string> descriptions {};

   auto& processModuleWarningsEnabled =
      settings::GeneralSettings::Instance().process_module_warnings_enabled();

   if (EnumProcessModules(process, modules, sizeof(modules), &cbNeeded))
   {
      std::uint32_t numModules = cbNeeded / sizeof(HMODULE);
      for (std::uint32_t i = 0; i < numModules; ++i)
      {
         char modulePath[MAX_PATH];
         if (GetModuleFileNameExA(process, modules[i], modulePath, MAX_PATH))
         {
            std::string path = modulePath;

            logger_->trace("DLL Found: {}", path);

            if (boost::algorithm::iends_with(path, "NahimicOSD.dll"))
            {
               std::string description =
                  QObject::tr(
                     "ASUS Sonic Studio injects a Nahimic driver, which causes "
                     "Supercell Wx to hang. It is suggested to disable the "
                     "Nahimic service, or to uninstall ASUS Sonic Studio and "
                     "the Nahimic driver.")
                     .toStdString();

               logger_->warn("Incompatible DLL found: {}", path);
               logger_->warn("{}", description);

               // Only populate vectors for the message box if warnings are
               // enabled
               if (processModuleWarningsEnabled.GetValue())
               {
                  incompatibleDlls.push_back(path);
                  descriptions.push_back(description);
               }
            }
         }
      }
   }

   if (!incompatibleDlls.empty())
   {
      const std::string header =
         QObject::tr(
            "The following DLLs have been injected into the Supercell Wx "
            "process:")
            .toStdString();
      const std::string defaultMessage =
         QObject::tr(
            "Supercell Wx is known to not run correctly with these DLLs "
            "injected. We suggest stopping or uninstalling these services if "
            "you experience crashes or unexpected behavior while using "
            "Supercell Wx.")
            .toStdString();

      std::string message = fmt::format("{}\n\n{}\n\n{}\n\n{}",
                                        header,
                                        fmt::join(incompatibleDlls, "\n"),
                                        defaultMessage,
                                        fmt::join(descriptions, "\n"));

      QMessageBox dialog(QMessageBox::Icon::Warning,
                         QObject::tr("Supercell Wx"),
                         QString::fromStdString(message));
      QCheckBox*  checkBox =
         new QCheckBox(QObject::tr("Don't show this message again"), &dialog);
      dialog.setCheckBox(checkBox);
      dialog.exec();

      // Stage the result of the checkbox. This value will be committed on
      // shutdown.
      processModuleWarningsEnabled.StageValue(!checkBox->isChecked());
   }
#endif
}

} // namespace scwx::qt::main
