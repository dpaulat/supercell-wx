#pragma once

#include <QLineEdit>

namespace scwx
{
namespace qt
{
namespace ui
{

class HotkeyEdit : public QLineEdit
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(HotkeyEdit)

public:
   explicit HotkeyEdit(QWidget* parent = nullptr);
   ~HotkeyEdit();

   QKeySequence key_sequence() const;

   void set_key_sequence(const QKeySequence& sequence);

protected:
   void focusInEvent(QFocusEvent* e) override;
   void focusOutEvent(QFocusEvent* e) override;
   void keyPressEvent(QKeyEvent* e) override;
   void keyReleaseEvent(QKeyEvent* e) override;

signals:
   void KeySequenceChanged(const QKeySequence& sequence);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace ui
} // namespace qt
} // namespace scwx
