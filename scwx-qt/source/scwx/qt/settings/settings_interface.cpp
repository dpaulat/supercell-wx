#define SETTINGS_INTERFACE_IMPLEMENTATION

#include <scwx/qt/settings/settings_interface.hpp>
#include <scwx/qt/settings/settings_variable.hpp>

#include <QAbstractButton>
#include <QCoreApplication>
#include <QLineEdit>
#include <QSpinBox>
#include <QWidget>

namespace scwx
{
namespace qt
{
namespace settings
{

static const std::string logPrefix_ = "scwx::qt::settings::settings_interface";

template<class T>
class SettingsInterface<T>::Impl
{
public:
   explicit Impl()
   {
      context_->moveToThread(QCoreApplication::instance()->thread());
   }

   ~Impl() {}

   void UpdateEditWidget();
   void UpdateResetButton();

   SettingsVariable<T>* variable_ {nullptr};
   bool                 stagedValid_ {true};

   std::unique_ptr<QObject> context_ {std::make_unique<QObject>()};
   QWidget*                 editWidget_ {nullptr};
   QAbstractButton*         resetButton_ {nullptr};
};

template<class T>
SettingsInterface<T>::SettingsInterface() : p(std::make_unique<Impl>())
{
}
template<class T>
SettingsInterface<T>::~SettingsInterface() = default;

template<class T>
SettingsInterface<T>::SettingsInterface(SettingsInterface&&) noexcept = default;
template<class T>
SettingsInterface<T>&
SettingsInterface<T>::operator=(SettingsInterface&&) noexcept = default;

template<class T>
void SettingsInterface<T>::SetSettingsVariable(SettingsVariable<T>& variable)
{
   p->variable_ = &variable;
}

template<class T>
void SettingsInterface<T>::SetEditWidget(QWidget* widget)
{
   if (p->editWidget_ != nullptr)
   {
      QObject::disconnect(p->editWidget_, nullptr, p->context_.get(), nullptr);
   }

   p->editWidget_ = widget;

   if (QLineEdit* lineEdit = dynamic_cast<QLineEdit*>(widget))
   {
      if constexpr (std::is_same_v<T, std::string>)
      {
         // If the line is edited (not programatically changed), stage the new
         // value
         QObject::connect(lineEdit,
                          &QLineEdit::textEdited,
                          p->context_.get(),
                          [this](const QString& text)
                          {
                             // Attempt to stage the value
                             p->stagedValid_ =
                                p->variable_->StageValue(text.toStdString());
                             p->UpdateResetButton();
                          });
      }
   }
   else if (QSpinBox* spinBox = dynamic_cast<QSpinBox*>(widget))
   {
      if constexpr (std::is_integral_v<T>)
      {
         const std::optional<T> minimum = p->variable_->GetMinimum();
         const std::optional<T> maximum = p->variable_->GetMaximum();

         if (minimum.has_value())
         {
            spinBox->setMinimum(static_cast<int>(*minimum));
         }
         if (maximum.has_value())
         {
            spinBox->setMaximum(static_cast<int>(*maximum));
         }

         // If the spin box is edited, stage a changed value
         QObject::connect(
            spinBox,
            &QSpinBox::valueChanged,
            p->context_.get(),
            [this](int i)
            {
               const T                value  = p->variable_->GetValue();
               const std::optional<T> staged = p->variable_->GetStaged();

               // If there is a value staged, and the new value is the same as
               // the current value, reset the staged value
               if (staged.has_value() && static_cast<T>(i) == value)
               {
                  p->variable_->Reset();
                  p->stagedValid_ = true;
                  p->UpdateResetButton();
               }
               // If there is no staged value, or if the new value is different
               // than what is staged, attempt to stage the value
               else if (!staged.has_value() || static_cast<T>(i) != *staged)
               {
                  p->stagedValid_ = p->variable_->StageValue(static_cast<T>(i));
                  p->UpdateResetButton();
               }
               // Otherwise, don't process an unchanged value
            });
      }
   }

   p->UpdateEditWidget();
}

template<class T>
void SettingsInterface<T>::SetResetButton(QAbstractButton* button)
{
   if (p->resetButton_ != nullptr)
   {
      QObject::disconnect(p->resetButton_, nullptr, p->context_.get(), nullptr);
   }

   p->resetButton_ = button;

   QObject::connect(p->resetButton_,
                    &QAbstractButton::clicked,
                    p->context_.get(),
                    [this]()
                    {
                       T defaultValue = p->variable_->GetDefault();

                       if (p->variable_->GetValue() == defaultValue)
                       {
                          // If the current value is default, reset the staged
                          // value
                          p->variable_->Reset();
                          p->stagedValid_ = true;
                          p->UpdateEditWidget();
                          p->UpdateResetButton();
                       }
                       else
                       {
                          // Stage the default value
                          p->stagedValid_ =
                             p->variable_->StageValue(defaultValue);
                          p->UpdateEditWidget();
                          p->UpdateResetButton();
                       }
                    });

   p->UpdateResetButton();
}

template<class T>
void SettingsInterface<T>::Impl::UpdateEditWidget()
{
   // Use the staged value if present, otherwise the current value
   const std::optional<T> staged       = variable_->GetStaged();
   const T                value        = variable_->GetValue();
   const T&               displayValue = staged.has_value() ? *staged : value;

   if (QLineEdit* lineEdit = dynamic_cast<QLineEdit*>(editWidget_))
   {
      if constexpr (std::is_integral_v<T>)
      {
         lineEdit->setText(QString::number(displayValue));
      }
      else if constexpr (std::is_same_v<T, std::string>)
      {
         lineEdit->setText(QString::fromStdString(displayValue));
      }
   }
   else if (QSpinBox* spinBox = dynamic_cast<QSpinBox*>(editWidget_))
   {
      if constexpr (std::is_integral_v<T>)
      {
         spinBox->setValue(static_cast<int>(displayValue));
      }
   }
}

template<class T>
void SettingsInterface<T>::Impl::UpdateResetButton()
{
   const std::optional<T> staged       = variable_->GetStaged();
   const T                defaultValue = variable_->GetDefault();
   const T                value        = variable_->GetValue();

   if (resetButton_ != nullptr)
   {
      if (staged.has_value())
      {
         resetButton_->setVisible(!stagedValid_ || *staged != defaultValue);
      }
      else
      {
         resetButton_->setVisible(value != defaultValue);
      }
   }
}

} // namespace settings
} // namespace qt
} // namespace scwx
