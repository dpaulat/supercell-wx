#include <scwx/qt/ui/keybind_edit.hpp>
#include <scwx/util/logger.hpp>

#include <qevent.h>

namespace scwx
{
namespace qt
{
namespace ui
{

static const std::string logPrefix_ = "scwx::qt::ui::keybind_edit";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class KeybindEdit::Impl
{
public:
   explicit Impl() {};
   ~Impl() = default;

   QKeySequence sequence_ {};
};

KeybindEdit::KeybindEdit(QWidget* parent) :
    QLineEdit(parent), p {std::make_unique<Impl>()}
{
   setReadOnly(true);
   setClearButtonEnabled(true);

   QAction* clearAction = findChild<QAction*>();
   if (clearAction != nullptr)
   {
      clearAction->setEnabled(true);

      connect(clearAction,
              &QAction::triggered,
              this,
              [this](bool /* checked */)
              {
                 logger_->trace("clearAction");

                 if (!p->sequence_.isEmpty())
                 {
                    // Clear saved sequence
                    p->sequence_ = QKeySequence {};
                    setText(p->sequence_.toString());
                    Q_EMIT KeySequenceChanged({});
                 }
              });
   }
}

KeybindEdit::~KeybindEdit() {}

QKeySequence KeybindEdit::key_sequence() const
{
   return p->sequence_;
}

void KeybindEdit::set_key_sequence(const QKeySequence& sequence)
{
   if (sequence != p->sequence_)
   {
      p->sequence_ = sequence;
      setText(sequence.toString());
      Q_EMIT KeySequenceChanged(sequence);
   }
}

void KeybindEdit::focusInEvent(QFocusEvent* e)
{
   logger_->trace("focusInEvent");

   // Replace text with placeholder prompting for input
   setPlaceholderText("Press any key");
   setText({});

   QLineEdit::focusInEvent(e);
}

void KeybindEdit::focusOutEvent(QFocusEvent* e)
{
   logger_->trace("focusOutEvent");

   // Replace text with saved sequence
   setPlaceholderText({});
   setText(p->sequence_.toString());

   QLineEdit::focusOutEvent(e);
}

void KeybindEdit::keyPressEvent(QKeyEvent* e)
{
   logger_->trace("keyPressEvent");

   QKeySequence sequence {};

   switch (e->key())
   {
   case Qt::Key::Key_Shift:
   case Qt::Key::Key_Control:
   case Qt::Key::Key_Alt:
   case Qt::Key::Key_Meta:
   case Qt::Key::Key_Mode_switch:
      // Record modifiers only in sequence
      sequence = e->modifiers().toInt();
      break;

   default:
      // Record modifiers and keys in sequence, and save sequence
      sequence = e->modifiers().toInt() | e->key();

      if (sequence != p->sequence_)
      {
         p->sequence_ = sequence;
         Q_EMIT KeySequenceChanged(sequence);
      }

      clearFocus();
      break;
   }

   setText(sequence.toString());
}

void KeybindEdit::keyReleaseEvent(QKeyEvent*)
{
   logger_->trace("keyReleaseEvent");

   // Modifiers were released prior to pressing a non-modifier key
   setText({});
}

} // namespace ui
} // namespace qt
} // namespace scwx
