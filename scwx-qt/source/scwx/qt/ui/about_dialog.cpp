#include "about_dialog.hpp"
#include "ui_about_dialog.h"
#include <scwx/qt/main/versions.hpp>
#include <scwx/qt/manager/font_manager.hpp>

#include <QFontDatabase>

namespace scwx
{
namespace qt
{
namespace ui
{

class AboutDialogImpl
{
public:
   explicit AboutDialogImpl() = default;
   ~AboutDialogImpl()         = default;
};

AboutDialog::AboutDialog(QWidget* parent) :
    QDialog(parent),
    p {std::make_unique<AboutDialogImpl>()},
    ui(new Ui::AboutDialog)
{
   ui->setupUi(this);

   int titleFontId =
      manager::FontManager::Instance().GetFontId(types::Font::din1451alt_g);
   QString titleFontFamily =
      QFontDatabase::applicationFontFamilies(titleFontId).at(0);
   QFont titleFont(titleFontFamily, 14);
   ui->titleLabel->setFont(titleFont);

   QString repositoryUrl =
      QString("https://github.com/dpaulat/supercell-wx/tree/%1")
         .arg(QString::fromStdString(main::kCommitString_));

   // Remove +dirty from the URL
   qsizetype delimiter = repositoryUrl.indexOf('+');
   if (delimiter != -1)
   {
      repositoryUrl = repositoryUrl.left(delimiter);
   }

   ui->versionLabel->setText(
      tr("Version %1").arg(QString::fromStdString(main::kVersionString_)));
   ui->revisionLabel->setText(
      tr("Git Revision <a href=\"%1\">%2</a>")
         .arg(repositoryUrl)
         .arg(QString::fromStdString(main::kCommitString_)));
   ui->copyrightLabel->setText(
      tr("Copyright \302\251 2021-%1 Dan Paulat").arg(main::kCopyrightYear_));

   ui->revisionLabel->setOpenExternalLinks(true);
}

AboutDialog::~AboutDialog()
{
   delete ui;
}

} // namespace ui
} // namespace qt
} // namespace scwx
