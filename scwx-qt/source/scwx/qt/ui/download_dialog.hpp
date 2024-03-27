#pragma once

#include <scwx/qt/ui/progress_dialog.hpp>

#include <cstddef>

namespace scwx
{
namespace qt
{
namespace ui
{
class DownloadDialog : public ProgressDialog
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(DownloadDialog)

public:
   explicit DownloadDialog(QWidget* parent = nullptr);
   ~DownloadDialog();

   void set_filename(const std::string& filename);

public slots:
   void StartDownload();
   void UpdateProgress(std::ptrdiff_t downloadedBytes,
                       std::ptrdiff_t totalBytes);
   void FinishDownload();
   void CancelDownload();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace ui
} // namespace qt
} // namespace scwx
