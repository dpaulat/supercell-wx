#include "edit_button_dialog.hpp"
#include "ui_edit_button_dialog.h"

#include <scwx/qt/manager/font_manager.hpp>
#include <scwx/qt/ui/widgets/imgui_button.hpp>
#include <scwx/qt/util/color.hpp>
#include <scwx/util/logger.hpp>

#include <fmt/format.h>
#include <QColorDialog>

namespace scwx::qt::ui
{

static const std::string logPrefix_ = "scwx::qt::ui::edit_button_dialog";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class EditButtonDialog::Impl
{
public:
   struct EditComponent
   {
      void ConnectSignals(EditButtonDialog* self)
      {
         QObject::connect(colorLineEdit_,
                          &QLineEdit::textEdited,
                          self,
                          [self, this](const QString& text)
                          {
                             const boost::gil::rgba8_pixel_t color =
                                util::color::ToRgba8PixelT(text.toStdString());
                             self->p->set_color(*this, color, false);
                          });

         QObject::connect(colorButton_,
                          &QAbstractButton::clicked,
                          self,
                          [self, this]() { self->p->ShowColorDialog(*this); });
      }

      boost::gil::rgba8_pixel_t color_;
      QFrame*                   colorFrame_ {nullptr};
      QLineEdit*                colorLineEdit_ {nullptr};
      QToolButton*              colorButton_ {nullptr};
   };

   explicit Impl(EditButtonDialog* self) :
       self_ {self}, button_ {new ImGuiButton(self)}
   {
   }
   ~Impl() = default;

   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   void SetDefaults();
   void ShowColorDialog(EditComponent& component);
   void UpdateSampleButton();

   void set_color(EditComponent&                   component,
                  const boost::gil::rgba8_pixel_t& color,
                  bool                             updateLineEdit = true);

   static void SetBackgroundColor(const std::string& value, QFrame* frame);

   EditButtonDialog* self_;

   ImGuiButton* button_;

   // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
   boost::gil::rgba8_pixel_t defaultButtonColor_ {66, 150, 250, 102};
   boost::gil::rgba8_pixel_t defaultHoverColor_ {66, 150, 250, 255};
   boost::gil::rgba8_pixel_t defaultActiveColor_ {15, 135, 250, 255};
   // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)

   EditComponent buttonComponent_ {};
   EditComponent hoverComponent_ {};
   EditComponent activeComponent_ {};
};

EditButtonDialog::EditButtonDialog(QWidget* parent) :
    QDialog(parent),
    p {std::make_unique<Impl>(this)},
    ui(new Ui::EditButtonDialog)
{
   ui->setupUi(this);

   p->activeComponent_.colorFrame_    = ui->activeColorFrame;
   p->activeComponent_.colorLineEdit_ = ui->activeColorLineEdit;
   p->activeComponent_.colorButton_   = ui->activeColorButton;

   p->buttonComponent_.colorFrame_    = ui->buttonColorFrame;
   p->buttonComponent_.colorLineEdit_ = ui->buttonColorLineEdit;
   p->buttonComponent_.colorButton_   = ui->buttonColorButton;

   p->hoverComponent_.colorFrame_    = ui->hoverColorFrame;
   p->hoverComponent_.colorLineEdit_ = ui->hoverColorLineEdit;
   p->hoverComponent_.colorButton_   = ui->hoverColorButton;

   p->SetDefaults();

   auto font =
      manager::FontManager::Instance().GetQFont(types::FontCategory::Default);
   p->button_->setFont(font);
   p->button_->setText(tr("BUTTON TEXT"));

   p->UpdateSampleButton();

   // Type is known at compile-time
   // NOLINTBEGIN(cppcoreguidelines-pro-type-static-cast-downcast)
   auto buttonContainerLayout =
      static_cast<QHBoxLayout*>(ui->buttonContainer->layout());
   buttonContainerLayout->insertWidget(1, p->button_);
   // NOLINTEND(cppcoreguidelines-pro-type-static-cast-downcast)

   p->activeComponent_.ConnectSignals(this);
   p->buttonComponent_.ConnectSignals(this);
   p->hoverComponent_.ConnectSignals(this);

   QObject::connect(ui->buttonBox,
                    &QDialogButtonBox::clicked,
                    this,
                    [this](QAbstractButton* button)
                    {
                       const QDialogButtonBox::ButtonRole role =
                          ui->buttonBox->buttonRole(button);

                       switch (role)
                       {
                       case QDialogButtonBox::ButtonRole::ResetRole: // Reset
                          p->SetDefaults();
                          break;

                       default:
                          break;
                       }
                    });
}

EditButtonDialog::~EditButtonDialog()
{
   delete ui;
}

boost::gil::rgba8_pixel_t EditButtonDialog::active_color() const
{
   return p->activeComponent_.color_;
}

boost::gil::rgba8_pixel_t EditButtonDialog::button_color() const
{
   return p->buttonComponent_.color_;
}

boost::gil::rgba8_pixel_t EditButtonDialog::hover_color() const
{
   return p->hoverComponent_.color_;
}

void EditButtonDialog::set_active_color(const boost::gil::rgba8_pixel_t& color)
{
   p->set_color(p->activeComponent_, color);
}

void EditButtonDialog::set_button_color(const boost::gil::rgba8_pixel_t& color)
{
   p->set_color(p->buttonComponent_, color);
}

void EditButtonDialog::set_hover_color(const boost::gil::rgba8_pixel_t& color)
{
   p->set_color(p->hoverComponent_, color);
}

void EditButtonDialog::Impl::set_color(EditComponent& component,
                                       const boost::gil::rgba8_pixel_t& color,
                                       bool updateLineEdit)
{
   // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
   static const boost::gil::rgba8_pixel_t kBlack {0, 0, 0, 255};

   const std::string argbString {util::color::ToArgbString(color)};
   const std::string blendedString {
      util::color::ToArgbString(util::color::Blend(color, kBlack))};

   component.color_ = color;
   SetBackgroundColor(blendedString, component.colorFrame_);

   if (updateLineEdit)
   {
      component.colorLineEdit_->setText(QString::fromStdString(argbString));
   }

   UpdateSampleButton();
}

void EditButtonDialog::Impl::UpdateSampleButton()
{
   button_->SetStyle(
      buttonComponent_.color_, hoverComponent_.color_, activeComponent_.color_);
}

void EditButtonDialog::Initialize(const boost::gil::rgba8_pixel_t& activeColor,
                                  const boost::gil::rgba8_pixel_t& buttonColor,
                                  const boost::gil::rgba8_pixel_t& hoverColor)
{
   p->defaultActiveColor_ = activeColor;
   p->defaultButtonColor_ = buttonColor;
   p->defaultHoverColor_  = hoverColor;

   p->SetDefaults();
}

void EditButtonDialog::Impl::SetDefaults()
{
   self_->set_active_color(defaultActiveColor_);
   self_->set_button_color(defaultButtonColor_);
   self_->set_hover_color(defaultHoverColor_);
}

void EditButtonDialog::Impl::ShowColorDialog(EditComponent& component)
{
   // NOLINTNEXTLINE(cppcoreguidelines-owning-memory): Owned by parent
   auto* dialog = new QColorDialog(self_);

   dialog->setAttribute(Qt::WA_DeleteOnClose);
   dialog->setOption(QColorDialog::ColorDialogOption::ShowAlphaChannel);

   const QColor initialColor(component.colorLineEdit_->text());
   if (initialColor.isValid())
   {
      dialog->setCurrentColor(initialColor);
   }

   QObject::connect(
      dialog,
      &QColorDialog::colorSelected,
      self_,
      [this, &component](const QColor& qColor)
      {
         const QString colorName = qColor.name(QColor::NameFormat::HexArgb);
         const boost::gil::rgba8_pixel_t color =
            util::color::ToRgba8PixelT(colorName.toStdString());

         logger_->info("Selected color: {}", colorName.toStdString());
         set_color(component, color);
      });

   dialog->open();
}

void EditButtonDialog::Impl::SetBackgroundColor(const std::string& value,
                                                QFrame*            frame)
{
   frame->setStyleSheet(
      QString::fromStdString(fmt::format("background-color: {}", value)));
}

} // namespace scwx::qt::ui
