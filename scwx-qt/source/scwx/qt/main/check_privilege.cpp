#include <scwx/qt/settings/general_settings.hpp>
#include <scwx/qt/main/check_privilege.hpp>
#include <QtGlobal>
#include <QStandardPaths>
#include <filesystem>
#include <QObject>
#include <QMessageBox>
#include <QCheckBox>

#ifdef _WIN32
#   include <windows.h>
#else
#   include <unistd.h>
#endif

namespace scwx::qt::main
{

bool is_high_privilege()
{
#if defined(_WIN32)
   bool            isAdmin = false;
   HANDLE          token   = NULL;
   TOKEN_ELEVATION elevation;
   DWORD           elevationSize = sizeof(TOKEN_ELEVATION);

   if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
   {
      return false;
   }
   if (!GetTokenInformation(
          token, TokenElevation, &elevation, elevationSize, &elevationSize))
   {
      CloseHandle(token);
      return false;
   }
   isAdmin = elevation.TokenIsElevated;
   CloseHandle(token);
   return isAdmin;
#elif defined(Q_OS_UNIX)
   // On UNIX root is always uid 0. On Linux this is enforced by the kernel.
   return geteuid() == 0;
#else
   return false;
#endif
}

#if defined(_WIN32)
static const QString message = QObject::tr(
   "Supercell Wx has been run with administrator permissions. It is "
   "recommended to run it without administrator permissions Do you wish to "
   "continue?");
#elif defined(Q_OS_UNIX)
static const QString message = QObject::tr(
   "Supercell Wx has been run as root. It is recommended to run it as a normal "
   "user. Do you wish to continue?");
#else
static const QString message = QObject::tr("");
#endif

static const QString title = QObject::tr("Supercell Wx");
static const QString checkBoxText =
   QObject::tr("Do not show this warning again.");

class PrivilegeChecker::Impl
{
public:
   explicit Impl() :
       highPrivilege_ {is_high_privilege()},
       dialog_ {QMessageBox::Icon::Warning, title, message},
       checkBox_(new QCheckBox(checkBoxText, &dialog_))
   {
      const std::string appDataPath {
         QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            .toStdString()};
      hasAppData_ = std::filesystem::exists(appDataPath);

      dialog_.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
      dialog_.setDefaultButton(QMessageBox::No);
      dialog_.setCheckBox(checkBox_);
   };

   bool hasAppData_;
   bool firstCheckCheckBoxState_ {true};
   bool highPrivilege_;

   QMessageBox dialog_;
   QCheckBox*  checkBox_;
};

PrivilegeChecker::PrivilegeChecker() :
    p(std::make_unique<PrivilegeChecker::Impl>())
{
}

PrivilegeChecker::~PrivilegeChecker() = default;

bool PrivilegeChecker::pre_settings_check()
{
   if (p->hasAppData_ || !p->highPrivilege_)
   {
      return false;
   }

   const int result            = p->dialog_.exec();
   p->firstCheckCheckBoxState_ = p->checkBox_->isChecked();

   return result != QMessageBox::Yes;
}

bool PrivilegeChecker::post_settings_check()
{
   auto& highPrivilegeWarningEnabled =
      settings::GeneralSettings::Instance().high_privilege_warning_enabled();
   if (!highPrivilegeWarningEnabled.GetValue() || !p->highPrivilege_)
   {
      return false;
   }
   else if (!p->hasAppData_)
   {
      highPrivilegeWarningEnabled.StageValue(!p->firstCheckCheckBoxState_);
      return false;
   }

   switch (p->dialog_.exec())
   {
   case QMessageBox::Yes:
      highPrivilegeWarningEnabled.StageValue(!p->checkBox_->isChecked());
      return false;
   case QMessageBox::No:
   default:
      return true;
   }
}

} // namespace scwx::qt::main
