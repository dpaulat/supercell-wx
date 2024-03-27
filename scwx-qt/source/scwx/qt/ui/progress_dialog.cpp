#include "progress_dialog.hpp"
#include "ui_progress_dialog.h"

namespace scwx
{
namespace qt
{
namespace ui
{

class ProgressDialog::Impl
{
public:
   explicit Impl() = default;
   ~Impl()         = default;
};

ProgressDialog::ProgressDialog(QWidget* parent) :
    QDialog(parent), p {std::make_unique<Impl>()}, ui(new Ui::ProgressDialog)
{
   ui->setupUi(this);
}

ProgressDialog::~ProgressDialog()
{
   delete ui;
}

QDialogButtonBox* ProgressDialog::button_box() const
{
   return ui->buttonBox;
}

void ProgressDialog::SetTopLabelText(const QString& text)
{
   ui->topLabel->setText(text);
}

void ProgressDialog::SetBottomLabelText(const QString& text)
{
   ui->bottomLabel->setText(text);
}

void ProgressDialog::SetMinimum(int minimum)
{
   ui->progressBar->setMinimum(minimum);
}

void ProgressDialog::SetMaximum(int maximum)
{
   ui->progressBar->setMaximum(maximum);
}

void ProgressDialog::SetRange(int minimum, int maximum)
{
   ui->progressBar->setRange(minimum, maximum);
}

void ProgressDialog::SetValue(int value)
{
   ui->progressBar->setValue(value);
}

} // namespace ui
} // namespace qt
} // namespace scwx
