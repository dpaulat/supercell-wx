#include <scwx/qt/ui/setup/audio_codec_page.hpp>
#include <scwx/qt/manager/settings_manager.hpp>
#include <scwx/qt/settings/audio_settings.hpp>
#include <scwx/qt/settings/settings_interface.hpp>

#include <QCheckBox>
#include <QDesktopServices>
#include <QLabel>
#include <QMediaFormat>
#include <QScrollArea>
#include <QSpacerItem>
#include <QVBoxLayout>

namespace scwx
{
namespace qt
{
namespace ui
{
namespace setup
{

class AudioCodecPage::Impl
{
public:
   explicit Impl(AudioCodecPage* self) : self_ {self} {};
   ~Impl() = default;

   void SetupSettingsInterface();
   void SetInstructionsLabelText();

   AudioCodecPage* self_;

   QLayout* layout_ {};
   QLayout* topLayout_ {};

   QScrollArea* scrollArea_ {};
   QWidget*     contents_ {};
   QLabel*      descriptionLabel_ {};
   QLabel*      instructionsLabel_ {};
   QCheckBox*   ignoreMissingCodecsCheckBox_ {};
   QSpacerItem* spacer_ {};

   settings::SettingsInterface<bool> ignoreMissingCodecs_ {};
};

AudioCodecPage::AudioCodecPage(QWidget* parent) :
    QWizardPage(parent), p {std::make_shared<Impl>(this)}
{
   setTitle(tr("Media Codecs"));
   setSubTitle(tr("Configure system media settings for Supercell Wx."));

   p->descriptionLabel_            = new QLabel(this);
   p->instructionsLabel_           = new QLabel(this);
   p->ignoreMissingCodecsCheckBox_ = new QCheckBox(this);

   // Description
   p->descriptionLabel_->setText(
      tr("Your system does not have the proper codecs installed in order to "
         "play the default audio. You may either install the proper codecs, or "
         "update Supercell Wx audio settings to change from the default audio "
         "files. After installing the proper codecs, you must restart "
         "Supercell Wx."));
   p->descriptionLabel_->setWordWrap(true);
   p->SetInstructionsLabelText();
   p->instructionsLabel_->setWordWrap(true);

   p->ignoreMissingCodecsCheckBox_->setText(tr("Ignore missing codecs"));

   p->spacer_ =
      new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Expanding);

   // Overall layout
   p->layout_ = new QVBoxLayout(this);
   p->layout_->addWidget(p->descriptionLabel_);
   p->layout_->addWidget(p->instructionsLabel_);
   p->layout_->addWidget(p->ignoreMissingCodecsCheckBox_);
   p->layout_->addItem(p->spacer_);

   p->contents_ = new QWidget(this);
   p->contents_->setLayout(p->layout_);

   p->scrollArea_ = new QScrollArea(this);
   p->scrollArea_->setHorizontalScrollBarPolicy(
      Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
   p->scrollArea_->setFrameShape(QFrame::Shape::NoFrame);
   p->scrollArea_->setWidgetResizable(true);
   p->scrollArea_->setWidget(p->contents_);

   p->topLayout_ = new QVBoxLayout(this);
   p->topLayout_->setContentsMargins(0, 0, 0, 0);
   p->topLayout_->addWidget(p->scrollArea_);

   setLayout(p->topLayout_);

   // Configure settings interface
   p->SetupSettingsInterface();
}

AudioCodecPage::~AudioCodecPage() = default;

void AudioCodecPage::Impl::SetInstructionsLabelText()
{
#if defined(_WIN32)
   instructionsLabel_->setText(tr(
      "<p><b>Option 1</b></p>" //
      "<p>Update your Windows installation. The required media codecs may "
      "be available with the latest operating system updates.</p>" //
      "<p><b>Option 2</b></p>"                                     //
      "<p>Install the <a "
      "href=\"https://www.microsoft.com/store/productId/9N5TDP8VCMHS\">Web "
      "Media Extensions</a> package from the Windows Store.</p>" //
      "<p><b>Option 3</b></p>"                                   //
      "<p>Install <a "
      "href=\"https://www.codecguide.com/"
      "download_k-lite_codec_pack_basic.htm\">K-Lite Codec Pack "
      "Basic</a>. This is a 3rd party application, and no support or warranty "
      "is provided.</p>"));
   instructionsLabel_->setTextInteractionFlags(
      Qt::TextInteractionFlag::TextBrowserInteraction);

   QObject::connect(instructionsLabel_,
                    &QLabel::linkActivated,
                    self_,
                    [](const QString& link)
                    { QDesktopServices::openUrl(QUrl {link}); });
#else
   instructionsLabel_->setText(
      tr("Please see the instructions for your Linux distribution for "
         "installing media codecs."));
#endif
}

void AudioCodecPage::Impl::SetupSettingsInterface()
{
   auto& audioSettings = settings::AudioSettings::Instance();

   ignoreMissingCodecs_.SetSettingsVariable(
      audioSettings.ignore_missing_codecs());
   ignoreMissingCodecs_.SetEditWidget(ignoreMissingCodecsCheckBox_);
}

bool AudioCodecPage::validatePage()
{
   bool committed = false;

   committed |= p->ignoreMissingCodecs_.Commit();

   if (committed)
   {
      manager::SettingsManager::Instance().SaveSettings();
   }

   return true;
}

bool AudioCodecPage::IsRequired()
{
   auto& audioSettings = settings::AudioSettings::Instance();

   bool ignoreCodecErrors = audioSettings.ignore_missing_codecs().GetValue();

   QMediaFormat oggFormat {QMediaFormat::FileFormat::Ogg};
   auto         oggCodecs =
      oggFormat.supportedAudioCodecs(QMediaFormat::ConversionMode::Decode);

   // Setup is required if codec errors are not ignored, and no Ogg support
   // is found.
   return (!ignoreCodecErrors && oggCodecs.empty());
}

} // namespace setup
} // namespace ui
} // namespace qt
} // namespace scwx
