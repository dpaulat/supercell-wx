#include <scwx/qt/ui/download_dialog.hpp>
#include <scwx/util/strings.hpp>

#include <boost/timer/timer.hpp>
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QTimer>

namespace scwx
{
namespace qt
{
namespace ui
{

class DownloadDialog::Impl
{
public:
   explicit Impl(DownloadDialog* self) : self_ {self}
   {
      updateTimer_.setSingleShot(true);
      updateTimer_.setInterval(0);

      QObject::connect(&updateTimer_,
                       &QTimer::timeout,
                       self_,
                       [this]() { UpdateProgress(); });
   };
   ~Impl() = default;

   void UpdateProgress();

   DownloadDialog* self_;

   boost::timer::cpu_timer timer_ {};
   QTimer                  updateTimer_ {};

   std::ptrdiff_t downloadedBytes_ {};
   std::ptrdiff_t totalBytes_ {};
};

DownloadDialog::DownloadDialog(QWidget* parent) :
    ProgressDialog(parent), p {std::make_unique<Impl>(this)}
{
   auto buttonBox = button_box();
   buttonBox->setStandardButtons(QDialogButtonBox::StandardButton::Ok |
                                 QDialogButtonBox::StandardButton::Cancel);
   buttonBox->button(QDialogButtonBox::StandardButton::Ok)
      ->setText("Install Now");

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

   SetBottomLabelText(tr("Waiting for download to begin..."));
   p->timer_.start();
   show();
}

void DownloadDialog::UpdateProgress(std::ptrdiff_t downloadedBytes,
                                    std::ptrdiff_t totalBytes)
{
   p->downloadedBytes_ = downloadedBytes;
   p->totalBytes_      = totalBytes;

   // Use a one-shot timer to trigger an update, preventing multiple updates per
   // frame
   p->updateTimer_.start();
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

void DownloadDialog::Impl::UpdateProgress()
{
   using namespace std::chrono_literals;

   const std::ptrdiff_t downloadedBytes = downloadedBytes_;
   const std::ptrdiff_t totalBytes      = totalBytes_;

   const std::chrono::nanoseconds elapsed {timer_.elapsed().wall};

   const double percentComplete =
      (totalBytes > 0.0) ? static_cast<double>(downloadedBytes) / totalBytes :
                           0.0;
   const int progressValue = static_cast<int>(percentComplete * 100.0);

   self_->SetValue(progressValue);

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

   self_->SetBottomLabelText(QString::fromStdString(progressText));
}

} // namespace ui
} // namespace qt
} // namespace scwx
