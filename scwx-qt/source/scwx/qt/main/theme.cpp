#include <scwx/qt/main/theme.hpp>
#include <scwx/qt/main/theme_internal.hpp>

#include <scwx/qt/settings/general_settings.hpp>
#include <scwx/qt/types/qt_types.hpp>
#include <scwx/util/logger.hpp>

#include <optional>
#include <QApplication>
#include <QPalette>
#include <QStyle>
#include <QStyleHints>

#define QT6CT_LIBRARY
#include <qt6ct-common/qt6ct.h>
#undef QT6CT_LIBRARY

namespace scwx::qt::main
{

static const std::string logPrefix_ = "scwx::qt::main::theme";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);
static bool              hasStyleArgument_ {false};

bool internal::HasStyleArgument(const std::vector<std::string>& args)
{
   for (std::size_t i = 1; i < args.size(); ++i)
   {
      if (args.at(i) == "-style")
      {
         return true;
      }
   }

   return false;
}

static void OverrideDefaultStyle()
{
#if defined(_WIN32)
   // Override the default Windows 11 style unless the user supplies a style
   // argument.
   if (!hasStyleArgument_)
   {
      QApplication::setStyle("windowsvista");
   }
#endif
}

static void ApplyThemeImpl()
{
   const scwx::qt::settings::GeneralSettings& generalSettings =
      scwx::qt::settings::GeneralSettings::Instance();

   const auto uiStyle =
      scwx::qt::types::GetUiStyle(generalSettings.theme().GetValue());
   const auto qtColorScheme = scwx::qt::types::GetQtColorScheme(uiStyle);

   if (uiStyle == scwx::qt::types::UiStyle::Default)
   {
      OverrideDefaultStyle();
   }
   else
   {
      QApplication::setStyle(
         QString::fromStdString(scwx::qt::types::GetQtStyleName(uiStyle)));
   }

   QGuiApplication::styleHints()->setColorScheme(qtColorScheme);

   std::optional<std::string> paletteFile;
   if (uiStyle == scwx::qt::types::UiStyle::FusionCustom)
   {
      paletteFile = generalSettings.theme_file().GetValue();
   }
   else
   {
      paletteFile = scwx::qt::types::GetQtPaletteFile(uiStyle);
   }

   if (paletteFile)
   {
      const QPalette defaultPalette = QApplication::style()->standardPalette();
      const QPalette palette        = Qt6CT::loadColorScheme(
         QString::fromStdString(*paletteFile), defaultPalette);

      if (defaultPalette == palette)
      {
         logger_->warn("Failed to load palette file '{}'", *paletteFile);
      }
      else
      {
         logger_->info("Loaded palette file '{}'", *paletteFile);
      }

      QApplication::setPalette(palette);
   }
   else
   {
      QApplication::setPalette(QApplication::style()->standardPalette());
   }
}

void ConfigureThemeForStartup(const std::vector<std::string>& args)
{
   hasStyleArgument_ = internal::HasStyleArgument(args);
   ApplyThemeImpl();
}

void ApplyTheme()
{
   ApplyThemeImpl();
}

} // namespace scwx::qt::main
