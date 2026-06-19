#pragma once

#include <QWizard>

namespace scwx::qt::ui::import
{

class ImportSettingsWizard : public QWizard
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(ImportSettingsWizard)

public:
   explicit ImportSettingsWizard(QWidget* parent = nullptr);
   ~ImportSettingsWizard() override;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::ui::import
