#pragma once

#include <QDialog>

#include <scwx/qt/types/text_event_key.hpp>
#include <scwx/common/geographic.hpp>

namespace Ui
{
class AlertDialog;
}

namespace scwx
{
namespace qt
{
namespace ui
{

class AlertDialogImpl;

class AlertDialog : public QDialog
{
   Q_OBJECT

public:
   explicit AlertDialog(QWidget* parent = nullptr);
   ~AlertDialog();

   bool SelectAlert(const types::TextEventKey& key,
                    const common::Coordinate&  coordinate);

signals:
   void MoveMap(double latitude, double longitude);

private:
   friend class AlertDialogImpl;
   std::unique_ptr<AlertDialogImpl> p;
   Ui::AlertDialog*                 ui;
};

} // namespace ui
} // namespace qt
} // namespace scwx
