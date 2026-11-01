#pragma once

#include <QKeyEvent>
#include <QSpinBox>
#include <QWheelEvent>

class QFocusedSpinBox : public QSpinBox
{
   Q_OBJECT

public:
   using QSpinBox::QSpinBox;

protected:
   // Keys the spin box does not consume would otherwise propagate to the main
   // window and trigger application hotkeys while typing a value.
   void keyPressEvent(QKeyEvent* event) override
   {
      QSpinBox::keyPressEvent(event);
      if (!IsNavigationKey(event->key()))
      {
         event->accept();
      }
   }

   void keyReleaseEvent(QKeyEvent* event) override
   {
      QSpinBox::keyReleaseEvent(event);
      if (!IsNavigationKey(event->key()))
      {
         event->accept();
      }
   }

   void wheelEvent(QWheelEvent* event) override
   {
      if (hasFocus())
      {
         QSpinBox::wheelEvent(event);
      }
      else
      {
         event->ignore();
      }
   }

private:
   static bool IsNavigationKey(int key)
   {
      return key == Qt::Key_Escape || key == Qt::Key_Tab ||
             key == Qt::Key_Backtab;
   }
};
