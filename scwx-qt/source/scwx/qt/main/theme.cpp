#include <scwx/qt/main/theme.hpp>
#include <scwx/qt/main/theme_internal.hpp>

#include <scwx/qt/settings/general_settings.hpp>
#include <scwx/qt/types/qt_types.hpp>
#include <scwx/util/logger.hpp>

#include <optional>

#include <QApplication>
#include <QPalette>
#include <QPixmapCache>
#include <QString>
#include <QStyle>
#include <QStyleHints>
#include <QWidget>

#define QT6CT_LIBRARY
#include <qt6ct-common/qt6ct.h>
#undef QT6CT_LIBRARY

namespace scwx::qt::main
{

static const std::string logPrefix_ = "scwx::qt::main::theme";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);
static bool              hasStyleArgument_ {false};

/** QStyleFactory key or empty if unknown (restore with setStyle(nullptr)). */
static bool     startupStyleKeyKnown_ {false};
static QString  startupStyleKey_;
static bool     startupPaletteKnown_ {false};
static QPalette startupPalette_;

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

#if defined(_WIN32)
static void OverrideDefaultStyle()
{
   // Override the default Windows 11 style unless the user supplies a style
   // argument.
   if (!hasStyleArgument_)
   {
      QApplication::setStyle("windowsvista");
   }
}
#endif

static void CaptureStartupStyleIfNeeded()
{
   if (startupStyleKeyKnown_)
   {
      return;
   }
   QStyle* style = QApplication::style();
   if (style == nullptr)
   {
      startupStyleKeyKnown_ = true;
      return;
   }
   startupStyleKey_      = style->objectName();
   startupStyleKeyKnown_ = true;
}

static void RestoreStartupStyle()
{
   CaptureStartupStyleIfNeeded();
   if (!startupStyleKeyKnown_)
   {
      return;
   }
   if (startupStyleKey_.isEmpty())
   {
      QApplication::setStyle(static_cast<QStyle*>(nullptr));
   }
   else
   {
      QApplication::setStyle(startupStyleKey_);
   }
}

static void CaptureStartupPaletteIfNeeded()
{
   if (startupPaletteKnown_)
   {
      return;
   }

   startupPalette_      = QApplication::palette();
   startupPaletteKnown_ = true;
}

static void RefreshApplicationWidgets()
{
   QPixmapCache::clear();

   for (QWidget* widget : QApplication::allWidgets())
   {
      QStyle* style = widget->style();
      if (style != nullptr)
      {
         style->unpolish(widget);
         style->polish(widget);
      }
      widget->update();
   }
}

static void ApplyThemeImpl()
{
   CaptureStartupStyleIfNeeded();
   CaptureStartupPaletteIfNeeded();

   const scwx::qt::settings::GeneralSettings& generalSettings =
      scwx::qt::settings::GeneralSettings::Instance();

   const auto uiStyle =
      scwx::qt::types::GetUiStyle(generalSettings.theme().GetValue());
   const auto qtColorScheme = scwx::qt::types::GetQtColorScheme(uiStyle);

   if (uiStyle == scwx::qt::types::UiStyle::Default)
   {
#if defined(_WIN32)
      if (!hasStyleArgument_)
      {
         OverrideDefaultStyle();
      }
      else
      {
         RestoreStartupStyle();
      }
#else
      RestoreStartupStyle();
#endif
   }
   else
   {
      const QString styleName =
         QString::fromStdString(scwx::qt::types::GetQtStyleName(uiStyle));

      if (QApplication::style() != nullptr &&
          QApplication::style()->objectName().compare(styleName,
                                                      Qt::CaseInsensitive) == 0)
      {
         // Recreate the style when only the color scheme changes (e.g. Fusion
         // Dark -> Fusion); otherwise Qt can keep the previous standard
         // palette.
         QApplication::setStyle(static_cast<QStyle*>(nullptr));
      }
      QApplication::setStyle(styleName);
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
      if (uiStyle == scwx::qt::types::UiStyle::Default && startupPaletteKnown_)
      {
         QApplication::setPalette(startupPalette_);
      }
      else if (uiStyle != scwx::qt::types::UiStyle::Default)
      {
         // Reset any palette loaded by a previous theme after the target
         // style/color scheme is active. A default-constructed palette has an
         // empty resolve mask, so Qt rebuilds from the current theme palette
         // instead of keeping stale explicit colors.
         QApplication::setPalette(QPalette {});
      }
   }

   RefreshApplicationWidgets();
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
