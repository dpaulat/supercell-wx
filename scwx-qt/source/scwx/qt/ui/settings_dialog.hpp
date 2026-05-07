#pragma once

#include <QDialog>
#include <qmaplibre.hpp>

namespace Ui
{
class SettingsDialog;
}

namespace scwx
{
namespace qt
{
namespace ui
{

class SettingsDialogImpl;

class SettingsDialog : public QDialog
{
   Q_OBJECT

private:
   Q_DISABLE_COPY(SettingsDialog)

signals:
   /// Emitted after OK or Apply successfully runs ApplyChanges() (settings
   /// committed in memory).
   void SettingsApplied();

public:
   explicit SettingsDialog(QMapLibre::Settings& mapSettings,
                           QWidget*             parent = nullptr);
   ~SettingsDialog();

   void RefreshWidgets();

private:
   friend SettingsDialogImpl;
   std::unique_ptr<SettingsDialogImpl> p;
   Ui::SettingsDialog*                 ui;
};

} // namespace ui
} // namespace qt
} // namespace scwx
