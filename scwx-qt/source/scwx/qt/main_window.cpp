#include "main_window.hpp"
#include "./ui_main_window.h"

namespace scwx
{
namespace qt
{

template<typename... Types>
class variant
{
public:
   template<typename T>
   inline variant(T&& value)
   {
   }
};

using ValueBase = variant<float, double>;

struct Value : ValueBase
{
   using ValueBase::ValueBase;
};

MainWindow::MainWindow(QWidget* parent) :
    QMainWindow(parent), ui(new Ui::MainWindow)
{
   ui->setupUi(this);

   Value(0.0f);
   Value(0.0);
}

MainWindow::~MainWindow()
{
   delete ui;
}

} // namespace qt
} // namespace scwx
