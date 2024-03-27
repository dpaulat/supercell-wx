#pragma once

#include <QDialog>

class QDialogButtonBox;

namespace Ui
{
class ProgressDialog;
}

namespace scwx
{
namespace qt
{
namespace ui
{
class ProgressDialog : public QDialog
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(ProgressDialog)

public:
   explicit ProgressDialog(QWidget* parent = nullptr);
   ~ProgressDialog();

protected:
   QDialogButtonBox* button_box() const;

public slots:
   void SetTopLabelText(const QString& text);
   void SetBottomLabelText(const QString& text);
   void SetMinimum(int minimum);
   void SetMaximum(int maximum);
   void SetRange(int minimum, int maximum);
   void SetValue(int value);

private:
   class Impl;
   std::unique_ptr<Impl> p;
   Ui::ProgressDialog*   ui;
};

} // namespace ui
} // namespace qt
} // namespace scwx
