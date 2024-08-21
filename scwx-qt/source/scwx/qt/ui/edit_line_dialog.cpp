#include "edit_line_dialog.hpp"
#include "ui_edit_line_dialog.h"

#include <scwx/qt/ui/line_label.hpp>
#include <scwx/qt/util/color.hpp>
#include <scwx/util/logger.hpp>

#include <fmt/format.h>
#include <QColorDialog>

namespace scwx
{
namespace qt
{
namespace ui
{

static const std::string logPrefix_ = "scwx::qt::ui::edit_line_dialog";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class EditLineDialog::Impl
{
public:
   struct EditComponent
   {
      void ConnectSignals(EditLineDialog* self)
      {
         QObject::connect(colorLineEdit_,
                          &QLineEdit::textEdited,
                          self,
                          [=, this](const QString& text)
                          {
                             boost::gil::rgba8_pixel_t color =
                                util::color::ToRgba8PixelT(text.toStdString());
                             self->p->set_color(*this, color);
                          });

         QObject::connect(colorButton_,
                          &QAbstractButton::clicked,
                          self,
                          [=, this]() { self->p->ShowColorDialog(*this); });

         QObject::connect(widthSpinBox_,
                          &QSpinBox::valueChanged,
                          self,
                          [=, this](int width)
                          { self->p->set_width(*this, width); });
      }

      boost::gil::rgba8_pixel_t color_;
      std::size_t               width_;
      QFrame*                   colorFrame_ {nullptr};
      QLineEdit*                colorLineEdit_ {nullptr};
      QToolButton*              colorButton_ {nullptr};
      QSpinBox*                 widthSpinBox_ {nullptr};
   };

   explicit Impl(EditLineDialog* self) :
       self_ {self}, lineLabel_ {new LineLabel(self)}
   {
   }
   ~Impl() = default;

   void SetDefaults();
   void ShowColorDialog(EditComponent& component);
   void UpdateLineLabel();

   void set_color(EditComponent& component, boost::gil::rgba8_pixel_t color);
   void set_width(EditComponent& component, std::size_t width);

   static void SetBackgroundColor(const std::string& value, QFrame* frame);

   EditLineDialog* self_;

   LineLabel* lineLabel_;

   boost::gil::rgba8_pixel_t defaultBorderColor_ {0, 0, 0, 255};
   boost::gil::rgba8_pixel_t defaultHighlightColor_ {0, 0, 0, 0};
   boost::gil::rgba8_pixel_t defaultLineColor_ {255, 255, 255, 255};

   std::size_t defaultBorderWidth_ {1u};
   std::size_t defaultHighlightWidth_ {0u};
   std::size_t defaultLineWidth_ {3u};

   EditComponent borderComponent_ {};
   EditComponent highlightComponent_ {};
   EditComponent lineComponent_ {};
};

EditLineDialog::EditLineDialog(QWidget* parent) :
    QDialog(parent),
    p {std::make_unique<Impl>(this)},
    ui(new Ui::EditLineDialog)
{
   ui->setupUi(this);

   p->borderComponent_.colorFrame_    = ui->borderColorFrame;
   p->borderComponent_.colorLineEdit_ = ui->borderColorLineEdit;
   p->borderComponent_.colorButton_   = ui->borderColorButton;
   p->borderComponent_.widthSpinBox_  = ui->borderWidthSpinBox;

   p->highlightComponent_.colorFrame_    = ui->highlightColorFrame;
   p->highlightComponent_.colorLineEdit_ = ui->highlightColorLineEdit;
   p->highlightComponent_.colorButton_   = ui->highlightColorButton;
   p->highlightComponent_.widthSpinBox_  = ui->highlightWidthSpinBox;

   p->lineComponent_.colorFrame_    = ui->lineColorFrame;
   p->lineComponent_.colorLineEdit_ = ui->lineColorLineEdit;
   p->lineComponent_.colorButton_   = ui->lineColorButton;
   p->lineComponent_.widthSpinBox_  = ui->lineWidthSpinBox;

   p->SetDefaults();

   p->lineLabel_->setMinimumWidth(72);

   QHBoxLayout* lineLabelContainerLayout =
      static_cast<QHBoxLayout*>(ui->lineLabelContainer->layout());
   lineLabelContainerLayout->insertWidget(1, p->lineLabel_);

   p->borderComponent_.ConnectSignals(this);
   p->highlightComponent_.ConnectSignals(this);
   p->lineComponent_.ConnectSignals(this);

   QObject::connect(ui->buttonBox,
                    &QDialogButtonBox::clicked,
                    this,
                    [this](QAbstractButton* button)
                    {
                       QDialogButtonBox::ButtonRole role =
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

EditLineDialog::~EditLineDialog()
{
   delete ui;
}

boost::gil::rgba8_pixel_t EditLineDialog::border_color() const
{
   return p->borderComponent_.color_;
}

boost::gil::rgba8_pixel_t EditLineDialog::highlight_color() const
{
   return p->highlightComponent_.color_;
}

boost::gil::rgba8_pixel_t EditLineDialog::line_color() const
{
   return p->lineComponent_.color_;
}

std::size_t EditLineDialog::border_width() const
{
   return p->borderComponent_.width_;
}

std::size_t EditLineDialog::highlight_width() const
{
   return p->highlightComponent_.width_;
}

std::size_t EditLineDialog::line_width() const
{
   return p->lineComponent_.width_;
}

void EditLineDialog::set_border_color(boost::gil::rgba8_pixel_t color)
{
   p->set_color(p->borderComponent_, color);
}

void EditLineDialog::set_highlight_color(boost::gil::rgba8_pixel_t color)
{
   p->set_color(p->highlightComponent_, color);
}

void EditLineDialog::set_line_color(boost::gil::rgba8_pixel_t color)
{
   p->set_color(p->lineComponent_, color);
}

void EditLineDialog::set_border_width(std::size_t width)
{
   p->set_width(p->borderComponent_, width);
}

void EditLineDialog::set_highlight_width(std::size_t width)
{
   p->set_width(p->highlightComponent_, width);
}

void EditLineDialog::set_line_width(std::size_t width)
{
   p->set_width(p->lineComponent_, width);
}

void EditLineDialog::Impl::set_color(EditComponent&            component,
                                     boost::gil::rgba8_pixel_t color)
{
   const std::string argbString {util::color::ToArgbString(color)};

   component.color_ = color;
   component.colorLineEdit_->setText(QString::fromStdString(argbString));
   SetBackgroundColor(argbString, component.colorFrame_);

   UpdateLineLabel();
}

void EditLineDialog::Impl::set_width(EditComponent& component,
                                     std::size_t    width)
{
   component.width_ = width;
   component.widthSpinBox_->setValue(static_cast<int>(width));

   UpdateLineLabel();
}

void EditLineDialog::Impl::UpdateLineLabel()
{
   lineLabel_->set_border_color(borderComponent_.color_);
   lineLabel_->set_highlight_color(highlightComponent_.color_);
   lineLabel_->set_line_color(lineComponent_.color_);

   lineLabel_->set_border_width(borderComponent_.width_);
   lineLabel_->set_highlight_width(highlightComponent_.width_);
   lineLabel_->set_line_width(lineComponent_.width_);
}

void EditLineDialog::Initialize(boost::gil::rgba8_pixel_t borderColor,
                                boost::gil::rgba8_pixel_t highlightColor,
                                boost::gil::rgba8_pixel_t lineColor,
                                std::size_t               borderWidth,
                                std::size_t               highlightWidth,
                                std::size_t               lineWidth)
{
   p->defaultBorderColor_    = borderColor;
   p->defaultHighlightColor_ = highlightColor;
   p->defaultLineColor_      = lineColor;

   p->defaultBorderWidth_    = borderWidth;
   p->defaultHighlightWidth_ = highlightWidth;
   p->defaultLineWidth_      = lineWidth;

   p->SetDefaults();
}

void EditLineDialog::Impl::SetDefaults()
{
   self_->set_border_color(defaultBorderColor_);
   self_->set_highlight_color(defaultHighlightColor_);
   self_->set_line_color(defaultLineColor_);

   self_->set_border_width(defaultBorderWidth_);
   self_->set_highlight_width(defaultHighlightWidth_);
   self_->set_line_width(defaultLineWidth_);
}

void EditLineDialog::Impl::ShowColorDialog(EditComponent& component)
{
   QColorDialog* dialog = new QColorDialog(self_);

   dialog->setAttribute(Qt::WA_DeleteOnClose);
   dialog->setOption(QColorDialog::ColorDialogOption::ShowAlphaChannel);

   QColor initialColor(component.colorLineEdit_->text());
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
         QString colorName = qColor.name(QColor::NameFormat::HexArgb);
         boost::gil::rgba8_pixel_t color =
            util::color::ToRgba8PixelT(colorName.toStdString());

         logger_->info("Selected color: {}", colorName.toStdString());
         set_color(component, color);
      });

   dialog->open();
}

void EditLineDialog::Impl::SetBackgroundColor(const std::string& value,
                                              QFrame*            frame)
{
   frame->setStyleSheet(
      QString::fromStdString(fmt::format("background-color: {}", value)));
}

} // namespace ui
} // namespace qt
} // namespace scwx
