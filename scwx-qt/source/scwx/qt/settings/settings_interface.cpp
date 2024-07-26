#define SETTINGS_INTERFACE_IMPLEMENTATION

#include <scwx/qt/settings/settings_interface.hpp>
#include <scwx/qt/settings/settings_variable.hpp>
#include <scwx/qt/ui/hotkey_edit.hpp>

#include <boost/tokenizer.hpp>
#include <fmt/ranges.h>
#include <QAbstractButton>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QLabel>
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
   explicit Impl(SettingsInterface* self) : self_ {self}
   {
      context_->moveToThread(QCoreApplication::instance()->thread());
   }

   ~Impl() {}

   template<class U>
   void SetWidgetText(U* widget, const T& currentValue);

   void UpdateEditWidget();
   void UpdateResetButton();
   void UpdateUnitLabel();

   SettingsInterface<T>* self_;

   SettingsVariable<T>* variable_ {nullptr};
   bool                 stagedValid_ {true};

   std::unique_ptr<QObject> context_ {std::make_unique<QObject>()};
   QWidget*                 editWidget_ {nullptr};
   QAbstractButton*         resetButton_ {nullptr};
   QLabel*                  unitLabel_ {nullptr};

   std::function<std::string(const T&)> mapFromValue_ {nullptr};
   std::function<T(const std::string&)> mapToValue_ {nullptr};

   double unitScale_ {1};
   const std::string * unitAbbreviation_ {nullptr};
   bool unitEnabled_ {false};
};

template<class T>
SettingsInterface<T>::SettingsInterface() :
    SettingsInterfaceBase(), p(std::make_unique<Impl>(this))
{
}
template<class T>
SettingsInterface<T>::~SettingsInterface() = default;

template<class T>
SettingsInterface<T>::SettingsInterface(SettingsInterface&& o) noexcept :
    p {std::move(o.p)}
{
   p->self_ = this;
}

template<class T>
SettingsInterface<T>&
SettingsInterface<T>::operator=(SettingsInterface&& o) noexcept
{
   p        = std::move(o.p);
   p->self_ = this;
   return *this;
}

template<class T>
void SettingsInterface<T>::SetSettingsVariable(SettingsVariable<T>& variable)
{
   p->variable_ = &variable;
}

template<class T>
SettingsVariable<T>* SettingsInterface<T>::GetSettingsVariable() const
{
   return p->variable_;
}

template<class T>
bool SettingsInterface<T>::IsDefault()
{
   bool isDefault = false;

   const std::optional<T> staged       = p->variable_->GetStaged();
   const T                defaultValue = p->variable_->GetDefault();
   const T                value        = p->variable_->GetValue();

   if (staged.has_value())
   {
      isDefault = (p->stagedValid_ && *staged == defaultValue);
   }
   else
   {
      isDefault = (value == defaultValue);
   }

   return isDefault;
}

template<class T>
bool SettingsInterface<T>::Commit()
{
   return p->variable_->Commit();
}

template<class T>
void SettingsInterface<T>::Reset()
{
   p->variable_->Reset();
   p->UpdateEditWidget();
   p->UpdateResetButton();
}

template<class T>
void SettingsInterface<T>::StageDefault()
{
   p->variable_->StageDefault();
   p->UpdateEditWidget();
   p->UpdateResetButton();
}

template<class T>
void SettingsInterface<T>::StageValue(const T& value)
{
   p->variable_->StageValue(value);
   p->UpdateEditWidget();
   p->UpdateResetButton();
}

template<class T>
void SettingsInterface<T>::SetEditWidget(QWidget* widget)
{
   if (p->editWidget_ != nullptr)
   {
      QObject::disconnect(p->editWidget_, nullptr, p->context_.get(), nullptr);
   }

   p->editWidget_ = widget;

   if (widget == nullptr)
   {
      return;
   }

   if (ui::HotkeyEdit* hotkeyEdit = dynamic_cast<ui::HotkeyEdit*>(widget))
   {
      if constexpr (std::is_same_v<T, std::string>)
      {
         QObject::connect(hotkeyEdit,
                          &ui::HotkeyEdit::KeySequenceChanged,
                          p->context_.get(),
                          [this](const QKeySequence& sequence)
                          {
                             std::string value {
                                sequence.toString().toStdString()};

                             // Attempt to stage the value
                             p->stagedValid_ = p->variable_->StageValue(value);
                             p->UpdateResetButton();

                             // TODO: Display invalid status
                          });
      }
   }
   else if (QLineEdit* lineEdit = dynamic_cast<QLineEdit*>(widget))
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
                             // Map to value if required
                             std::string value {text.toStdString()};
                             if (p->mapToValue_ != nullptr)
                             {
                                value = p->mapToValue_(value);
                             }

                             // Attempt to stage the value
                             p->stagedValid_ = p->variable_->StageValue(value);
                             p->UpdateResetButton();

                             // TODO: Display invalid status
                          });
      }
      else if constexpr (std::is_same_v<T, double>)
      {
         // If the line is edited (not programatically changed), stage the new
         // value
         QObject::connect(lineEdit,
                          &QLineEdit::textEdited,
                          p->context_.get(),
                          [this](const QString& text)
                          {
                             // Convert to a double
                             bool   ok;
                             double value = text.toDouble(&ok);
                             if (ok)
                             {
                                // Attempt to stage the value
                                p->stagedValid_ =
                                   p->variable_->StageValue(value);
                                p->UpdateResetButton();
                             }
                             else
                             {
                                p->stagedValid_ = false;
                                p->UpdateResetButton();
                             }
                          });
      }
      else if constexpr (std::is_same_v<T, std::vector<std::int64_t>>)
      {
         // If the line is edited (not programatically changed), stage the new
         // value
         QObject::connect(
            lineEdit,
            &QLineEdit::textEdited,
            p->context_.get(),
            [this](const QString& text)
            {
               // Map to value if required
               T value {};
               if (p->mapToValue_ != nullptr)
               {
                  // User-defined map to value
                  value = p->mapToValue_(text.toStdString());
               }
               else
               {
                  // Tokenize string to parse each element
                  const std::string str {text.toStdString()};
                  boost::tokenizer  tokens(str);
                  for (auto it = tokens.begin(); it != tokens.end(); ++it)
                  {
                     try
                     {
                        // Good value
                        value.push_back(
                           static_cast<T::value_type>(std::stoll(*it)));
                     }
                     catch (const std::exception&)
                     {
                        // Error value
                        value.push_back(
                           std::numeric_limits<typename T::value_type>::min());
                     }
                  }
               }

               // Attempt to stage the value
               p->stagedValid_ = p->variable_->StageValue(value);
               p->UpdateResetButton();

               // TODO: Display invalid status
            });
      }
   }
   else if (QCheckBox* checkBox = dynamic_cast<QCheckBox*>(widget))
   {
      if constexpr (std::is_same_v<T, bool>)
      {
         QObject::connect(checkBox,
                          &QCheckBox::toggled,
                          p->context_.get(),
                          [this](bool checked)
                          {
                             // Attempt to stage the value
                             p->stagedValid_ =
                                p->variable_->StageValue(checked);
                             p->UpdateResetButton();
                          });
      }
   }
   else if (QComboBox* comboBox = dynamic_cast<QComboBox*>(widget))
   {
      if constexpr (std::is_same_v<T, std::string>)
      {
         QObject::connect(comboBox,
                          &QComboBox::currentTextChanged,
                          p->context_.get(),
                          [this](const QString& text)
                          {
                             // Map to value if required
                             std::string value {text.toStdString()};
                             if (p->mapToValue_ != nullptr)
                             {
                                value = p->mapToValue_(value);
                             }

                             // Attempt to stage the value
                             p->stagedValid_ = p->variable_->StageValue(value);
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
   else if (QDoubleSpinBox* doubleSpinBox =
               dynamic_cast<QDoubleSpinBox*>(widget))
   {
      if constexpr (std::is_floating_point_v<T>)
      {
         const std::optional<T> minimum = p->variable_->GetMinimum();
         const std::optional<T> maximum = p->variable_->GetMaximum();

         if (minimum.has_value())
         {
            doubleSpinBox->setMinimum(static_cast<double>(*minimum));
         }
         if (maximum.has_value())
         {
            doubleSpinBox->setMaximum(static_cast<double>(*maximum));
         }

         // If the spin box is edited, stage a changed value
         QObject::connect(
            doubleSpinBox,
            &QDoubleSpinBox::valueChanged,
            p->context_.get(),
            [this](double d)
            {
               if (p->unitEnabled_)
               {
                  d = d / p->unitScale_;
               }

               const T                value  = p->variable_->GetValue();
               const std::optional<T> staged = p->variable_->GetStaged();

               // If there is a value staged, and the new value is the same as
               // the current value, reset the staged value
               if (staged.has_value() && static_cast<T>(d) == value)
               {
                  p->variable_->Reset();
                  p->stagedValid_ = true;
                  p->UpdateResetButton();
               }
               // If there is no staged value, or if the new value is different
               // than what is staged, attempt to stage the value
               else if (!staged.has_value() || static_cast<T>(d) != *staged)
               {
                  p->stagedValid_ = p->variable_->StageValue(static_cast<T>(d));
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

   if (p->resetButton_ != nullptr)
   {
      QObject::connect(p->resetButton_,
                       &QAbstractButton::clicked,
                       p->context_.get(),
                       [this]()
                       {
                          T defaultValue = p->variable_->GetDefault();

                          if (p->variable_->GetValue() == defaultValue)
                          {
                             // If the current value is default, reset the
                             // staged value
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
}
template<class T>
void SettingsInterface<T>::SetUnitLabel(QLabel* label)
{
   p->unitLabel_ = label;
}

template<class T>
void SettingsInterface<T>::SetMapFromValueFunction(
   std::function<std::string(const T&)> function)
{
   p->mapFromValue_ = function;
}

template<class T>
void SettingsInterface<T>::SetMapToValueFunction(
   std::function<T(const std::string&)> function)
{
   p->mapToValue_ = function;
}

template<class T>
void SettingsInterface<T>::SetUnit(const double&      scale,
                                   const std::string& abbreviation)
{
   p->unitScale_       = scale;
   p->unitAbbreviation_ = &abbreviation;
   p->unitEnabled_     = true;
   p->UpdateEditWidget();
   p->UpdateUnitLabel();
}

template<class T>
template<class U>
void SettingsInterface<T>::Impl::SetWidgetText(U* widget, const T& currentValue)
{
   if constexpr (std::is_integral_v<T>)
   {
      widget->setText(QString::number(currentValue));
   }
   else if constexpr (std::is_floating_point_v<T>)
   {
      widget->setText(QString::number(currentValue));
   }
   else if constexpr (std::is_same_v<T, std::string>)
   {
      if (mapFromValue_ != nullptr)
      {
         widget->setText(QString::fromStdString(mapFromValue_(currentValue)));
      }
      else
      {
         widget->setText(QString::fromStdString(currentValue));
      }
   }
   else if constexpr (std::is_same_v<T, std::vector<std::int64_t>>)
   {
      if (mapFromValue_ != nullptr)
      {
         widget->setText(QString::fromStdString(mapFromValue_(currentValue)));
      }
      else
      {
         widget->setText(QString::fromStdString(
            fmt::format("{}", fmt::join(currentValue, ", "))));
      }
   }
}

template<class T>
void SettingsInterface<T>::Impl::UpdateEditWidget()
{
   // Use the staged value if present, otherwise the current value
   const std::optional<T> staged       = variable_->GetStaged();
   const T                value        = variable_->GetValue();
   const T&               currentValue = staged.has_value() ? *staged : value;

   if (ui::HotkeyEdit* hotkeyEdit = dynamic_cast<ui::HotkeyEdit*>(editWidget_))
   {
      if constexpr (std::is_same_v<T, std::string>)
      {
         QKeySequence keySequence =
            QKeySequence::fromString(QString::fromStdString(currentValue));
         hotkeyEdit->set_key_sequence(keySequence);
      }
   }
   else if (QLineEdit* lineEdit = dynamic_cast<QLineEdit*>(editWidget_))
   {
      SetWidgetText(lineEdit, currentValue);
   }
   else if (QLabel* label = dynamic_cast<QLabel*>(editWidget_))
   {
      SetWidgetText(label, currentValue);
   }
   else if (QCheckBox* checkBox = dynamic_cast<QCheckBox*>(editWidget_))
   {
      if constexpr (std::is_same_v<T, bool>)
      {
         checkBox->setChecked(currentValue);
      }
   }
   else if (QComboBox* comboBox = dynamic_cast<QComboBox*>(editWidget_))
   {
      if constexpr (std::is_same_v<T, std::string>)
      {
         if (mapFromValue_ != nullptr)
         {
            comboBox->setCurrentText(
               QString::fromStdString(mapFromValue_(currentValue)));
         }
         else
         {
            comboBox->setCurrentText(QString::fromStdString(currentValue));
         }
      }
   }
   else if (QSpinBox* spinBox = dynamic_cast<QSpinBox*>(editWidget_))
   {
      if constexpr (std::is_integral_v<T>)
      {
         spinBox->setValue(static_cast<int>(currentValue));
      }
   }
   else if (QDoubleSpinBox* doubleSpinBox =
               dynamic_cast<QDoubleSpinBox*>(editWidget_))
   {
      if constexpr (std::is_floating_point_v<T>)
      {
         double doubleValue = static_cast<double>(currentValue);
         if (unitEnabled_)
         {
            doubleValue = doubleValue * unitScale_;
         }
         doubleSpinBox->setValue(doubleValue);
      }
   }
}

template<class T>
void SettingsInterface<T>::Impl::UpdateUnitLabel()
{
   if (unitLabel_ == nullptr || !unitEnabled_)
   {
      return;
   }

   unitLabel_->setText(QString::fromStdString(*unitAbbreviation_));
}

template<class T>
void SettingsInterface<T>::Impl::UpdateResetButton()
{
   if (resetButton_ != nullptr)
   {
      resetButton_->setVisible(!self_->IsDefault());
   }
}

} // namespace settings
} // namespace qt
} // namespace scwx
