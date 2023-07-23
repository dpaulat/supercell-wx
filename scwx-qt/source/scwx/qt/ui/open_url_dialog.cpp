#include "open_url_dialog.hpp"
#include "ui_open_url_dialog.h"

#include <scwx/util/logger.hpp>

#include <QFileDialog>
#include <QPushButton>

namespace scwx
{
namespace qt
{
namespace ui
{

static const std::string logPrefix_ = "scwx::qt::ui::open_url_dialog";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class OpenUrlDialogImpl
{
public:
   explicit OpenUrlDialogImpl(OpenUrlDialog* self) : self_ {self} {}
   ~OpenUrlDialogImpl() = default;

   void ConnectSignals();
   void SelectFile();

   OpenUrlDialog* self_;
};

OpenUrlDialog::OpenUrlDialog(const QString& title, QWidget* parent) :
    QDialog(parent),
    p {std::make_unique<OpenUrlDialogImpl>(this)},
    ui(new Ui::OpenUrlDialog)
{
   ui->setupUi(this);

   setWindowTitle(title);

   ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);

   p->ConnectSignals();
}

OpenUrlDialog::~OpenUrlDialog()
{
   delete ui;
}

void OpenUrlDialogImpl::ConnectSignals()
{
   QObject::connect(self_->ui->urlEdit,
                    &QLineEdit::textChanged,
                    self_,
                    [this](const QString& text)
                    {
                       QUrl url(text);
                       self_->ui->buttonBox->button(QDialogButtonBox::Ok)
                          ->setEnabled(url.isValid());
                    });

   QObject::connect(self_->ui->fileButton,
                    &QToolButton::clicked,
                    self_,
                    [this]() { SelectFile(); });
}

void OpenUrlDialog::showEvent(QShowEvent* event)
{
   ui->urlEdit->setText(QString());
   QDialog::showEvent(event);
}

QString OpenUrlDialog::url() const
{
   return ui->urlEdit->text();
}

void OpenUrlDialogImpl::SelectFile()
{
   static const std::string placefileFilter = "Placefiles (*)";

   QFileDialog* dialog = new QFileDialog(self_);

   dialog->setFileMode(QFileDialog::ExistingFile);
   dialog->setNameFilter(QObject::tr(placefileFilter.c_str()));
   dialog->setAttribute(Qt::WA_DeleteOnClose);

   QObject::connect(dialog,
                    &QFileDialog::fileSelected,
                    self_,
                    [this](const QString& file)
                    {
                       logger_->debug("Selected: {}", file.toStdString());
                       self_->ui->urlEdit->setText(file);
                    });

   dialog->open();
}

} // namespace ui
} // namespace qt
} // namespace scwx
