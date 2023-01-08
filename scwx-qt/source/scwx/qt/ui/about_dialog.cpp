#include "about_dialog.hpp"
#include "ui_about_dialog.h"
#include <scwx/qt/manager/resource_manager.hpp>

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
      manager::ResourceManager::FontId(types::Font::din1451alt_g);
   QString titleFontFamily =
      QFontDatabase::applicationFontFamilies(titleFontId).at(0);
   QFont titleFont(titleFontFamily, 14);
   ui->titleLabel->setFont(titleFont);
}

AboutDialog::~AboutDialog()
{
   delete ui;
}

} // namespace ui
} // namespace qt
} // namespace scwx
