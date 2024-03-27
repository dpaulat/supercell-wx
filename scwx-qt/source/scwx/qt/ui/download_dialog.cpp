#include <scwx/qt/ui/download_dialog.hpp>
#include <scwx/util/strings.hpp>

#include <boost/timer/timer.hpp>
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <QDialogButtonBox>
#include <QPushButton>

namespace scwx
{
namespace qt
{
namespace ui
{

class DownloadDialog::Impl
{
public:
   explicit Impl() {};
   ~Impl() = default;

   boost::timer::cpu_timer timer_ {};
};

DownloadDialog::DownloadDialog(QWidget* parent) :
    ProgressDialog(parent), p {std::make_unique<Impl>()}
{
   auto buttonBox = button_box();
   buttonBox->setStandardButtons(QDialogButtonBox::StandardButton::Ok |
                                 QDialogButtonBox::StandardButton::Cancel);
   buttonBox->button(QDialogButtonBox::StandardButton::Ok)
      ->setText("Install Now");

   setWindowTitle(tr("Download File"));
   SetRange(0, 100);
}

DownloadDialog::~DownloadDialog() {}

void DownloadDialog::set_filename(const std::string& filename)
{
   QString label = tr("Downloading %1...").arg(filename.c_str());
   SetTopLabelText(label);
}

void DownloadDialog::StartDownload()
{
   // Hide the OK button until the download is finished
   button_box()
      ->button(QDialogButtonBox::StandardButton::Ok)
      ->setVisible(false);

   SetValue(0);
   SetBottomLabelText(tr("Waiting for download to begin..."));
   p->timer_.start();
   show();
}

void DownloadDialog::UpdateProgress(std::ptrdiff_t downloadedBytes,
                                    std::ptrdiff_t totalBytes)
{
   using namespace std::chrono_literals;

   const std::chrono::nanoseconds elapsed {p->timer_.elapsed().wall};

   const double percentComplete =
      (totalBytes > 0.0) ? static_cast<double>(downloadedBytes) / totalBytes :
                           0.0;
   const int progressValue = static_cast<int>(percentComplete * 100.0);

   SetValue(progressValue);

   const std::chrono::seconds timeRemaining =
      (percentComplete > 0.0) ?
         std::chrono::duration_cast<std::chrono::seconds>(
            elapsed / percentComplete - elapsed) :
         0s;
   const std::chrono::hours hoursRemaining =
      std::chrono::duration_cast<std::chrono::hours>(timeRemaining);

   const std::string progressText =
      fmt::format("{} of {} downloaded ({}:{:%M:%S} remaining)",
                  util::BytesToString(downloadedBytes),
                  util::BytesToString(totalBytes),
                  hoursRemaining.count(),
                  timeRemaining);

   SetBottomLabelText(QString::fromStdString(progressText));
}

void DownloadDialog::FinishDownload()
{
   button_box()->button(QDialogButtonBox::StandardButton::Ok)->setVisible(true);
}

void DownloadDialog::CancelDownload()
{
   SetValue(0);
   SetBottomLabelText(tr("Error occurred while downloading"));
}

} // namespace ui
} // namespace qt
} // namespace scwx
