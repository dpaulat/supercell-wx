#include "main_window.hpp"

#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include <QApplication>

int main(int argc, char* argv[])
{
   boost::log::core::get()->set_filter(boost::log::trivial::severity >=
                                       boost::log::trivial::debug);

   QApplication               a(argc, argv);
   scwx::qt::main::MainWindow w;
   w.show();
   return a.exec();
}
