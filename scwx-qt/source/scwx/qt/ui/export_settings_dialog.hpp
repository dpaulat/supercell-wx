#pragma once

#include <QDialog>

#include <boost/gil/typedefs.hpp>

namespace Ui
{
class ExportSettingsDialog;
}

namespace scwx::qt::ui
{

class ExportSettingsDialog : public QDialog
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(ExportSettingsDialog)

public:
   explicit ExportSettingsDialog(QWidget* parent = nullptr);
   ~ExportSettingsDialog() override;

private:
   class Impl;
   std::unique_ptr<Impl>     p;
   Ui::ExportSettingsDialog* ui;
};

} // namespace scwx::qt::ui
