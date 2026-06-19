#pragma once

#include <scwx/zip/zip_stream_reader.hpp>

#include <memory>

#include <QWizardPage>

namespace scwx::qt::ui::import
{

class ImportOptionsPage : public QWizardPage
{
   Q_DISABLE_COPY_MOVE(ImportOptionsPage)

public:
   explicit ImportOptionsPage(QWidget* parent = nullptr);
   ~ImportOptionsPage() override;

   [[nodiscard]] bool isComplete() const override;
   bool               validatePage() override;

   void
   set_settings_file(const std::string& settingsFilename,
                     const std::shared_ptr<zip::ZipStreamReader>& settingsFile);

private:
   class Impl;
   std::shared_ptr<Impl> p;
};

} // namespace scwx::qt::ui::import
