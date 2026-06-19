#pragma once

#include <scwx/zip/zip_stream_reader.hpp>

#include <memory>

#include <QWizardPage>

namespace scwx::qt::ui::import
{

class SelectFilePage : public QWizardPage
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(SelectFilePage)

public:
   explicit SelectFilePage(QWidget* parent = nullptr);
   ~SelectFilePage() override;

   [[nodiscard]] bool isComplete() const override;
   bool               validatePage() override;

signals:
   void FileSelected(const std::string&                           filename,
                     const std::shared_ptr<zip::ZipStreamReader>& settingsFile);

private:
   class Impl;
   std::shared_ptr<Impl> p;
};

} // namespace scwx::qt::ui::import
