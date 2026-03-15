#include <scwx/qt/settings/settings_interface.hpp>
#include <scwx/qt/settings/settings_variable.hpp>
#include <scwx/qt/ui/hotkey_edit.hpp>

#include <utility>
#include <vector>

#include <boost/tokenizer.hpp>
#include <fmt/ranges.h>
#include <QAbstractButton>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QLabel>
#include <QLineEdit>
#include <QSlider>
#include <QSpinBox>
#include <QWidget>

namespace scwx::qt::settings
{

static const std::string logPrefix_ = "scwx::qt::settings::settings_interface";

static const QString kValidStyleSheet_   = "";
static const QString kInvalidStyleSheet_ = "border: 2px solid red;";

template<class T>
class SettingsInterface<T>::Impl
{
public:
   explicit Impl(SettingsInterface* self) : self_ {self}
   {
      context_->moveToThread(QCoreApplication::instance()->thread());
   }

   ~Impl()                       = default;
   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   template<class U>
   void SetWidgetText(U* widget, const T& currentValue);

   void UpdateResetButton();
   void UpdateUnitLabel();
   void UpdateValidityDisplay();
   void UpdateWidget(QWidget* widget);
   void UpdateWidgets();

   SettingsInterface<T>* self_;

   SettingsVariable<T>* variable_ {nullptr};
   bool                 stagedValid_ {true};

   std::unique_ptr<QObject> context_ {std::make_unique<QObject>()};
   QWidget*                 editWidget_ {nullptr};
   QWidget*                 labelWidget_ {nullptr};
   QAbstractButton*         resetButton_ {nullptr};
   QLabel*                  unitLabel_ {nullptr};

   std::function<std::string(const T&)> mapFromValue_ {nullptr};
   std::function<T(const std::string&)> mapToValue_ {nullptr};

   double                     unitScale_ {1};
   std::optional<std::string> unitAbbreviation_ {};
   bool                       unitEnabled_ {false};

   bool trimmingEnabled_ {false};

   std::optional<std::string> invalidTooltip_;
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
   p->UpdateWidgets();
   p->UpdateResetButton();
}

template<class T>
void SettingsInterface<T>::StageDefault()
{
   p->variable_->StageDefault();
   p->UpdateWidgets();
   p->UpdateResetButton();
}

template<class T>
void SettingsInterface<T>::StageValue(const T& value)
{
   p->variable_->StageValue(value);
   p->UpdateWidgets();
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

   if (auto* hotkeyEdit = dynamic_cast<ui::HotkeyEdit*>(widget))
   {
      if constexpr (std::is_same_v<T, std::string>)
      {
         QObject::connect(hotkeyEdit,
                          &ui::HotkeyEdit::KeySequenceChanged,
                          p->context_.get(),
                          [this](const QKeySequence& sequence)
                          {
                             const std::string value {
                                sequence.toString().toStdString()};

                             // Attempt to stage the value
                             p->stagedValid_ = p->variable_->StageValue(value);
                             p->UpdateWidget(p->labelWidget_);
                             p->UpdateResetButton();
                             p->UpdateValidityDisplay();
                          });
      }
   }
   else if (auto* lineEdit = dynamic_cast<QLineEdit*>(widget))
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
                             const QString trimmedText =
                                p->trimmingEnabled_ ? text.trimmed() : text;

                             // Map to value if required
                             std::string value {trimmedText.toStdString()};
                             if (p->mapToValue_ != nullptr)
                             {
                                value = p->mapToValue_(value);
                             }

                             // Attempt to stage the value
                             p->stagedValid_ = p->variable_->StageValue(value);
                             p->UpdateWidget(p->labelWidget_);
                             p->UpdateResetButton();
                             p->UpdateValidityDisplay();
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
                             bool         ok    = false;
                             const double value = text.toDouble(&ok);
                             if (ok)
                             {
                                // Attempt to stage the value
                                p->stagedValid_ =
                                   p->variable_->StageValue(value);
                                p->UpdateWidget(p->labelWidget_);
                                p->UpdateResetButton();
                             }
                             else
                             {
                                p->stagedValid_ = false;
                                p->UpdateWidget(p->labelWidget_);
                                p->UpdateResetButton();
                             }

                             p->UpdateValidityDisplay();
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
               p->UpdateWidget(p->labelWidget_);
               p->UpdateResetButton();
               p->UpdateValidityDisplay();
            });
      }
   }
   else if (auto* checkBox = dynamic_cast<QCheckBox*>(widget))
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
                             p->UpdateWidget(p->labelWidget_);
                             p->UpdateResetButton();
                          });
      }
   }
   else if (auto* comboBox = dynamic_cast<QComboBox*>(widget))
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
                             p->UpdateWidget(p->labelWidget_);
                             p->UpdateResetButton();
                          });
      }
   }
   else if (auto* spinBox = dynamic_cast<QSpinBox*>(widget))
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
                  p->UpdateWidget(p->labelWidget_);
                  p->UpdateResetButton();
               }
               // If there is no staged value, or if the new value is different
               // than what is staged, attempt to stage the value
               else if (!staged.has_value() || static_cast<T>(i) != *staged)
               {
                  p->stagedValid_ = p->variable_->StageValue(static_cast<T>(i));
                  p->UpdateWidget(p->labelWidget_);
                  p->UpdateResetButton();
               }
               // Otherwise, don't process an unchanged value

               p->UpdateValidityDisplay();
            });
      }
   }
   else if (auto* doubleSpinBox = dynamic_cast<QDoubleSpinBox*>(widget))
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
                  p->UpdateWidget(p->labelWidget_);
                  p->UpdateResetButton();
               }
               // If there is no staged value, or if the new value is different
               // than what is staged, attempt to stage the value
               else if (!staged.has_value() || static_cast<T>(d) != *staged)
               {
                  p->stagedValid_ = p->variable_->StageValue(static_cast<T>(d));
                  p->UpdateWidget(p->labelWidget_);
                  p->UpdateResetButton();
               }
               // Otherwise, don't process an unchanged value

               p->UpdateValidityDisplay();
            });
      }
   }
   else if (auto* slider = dynamic_cast<QSlider*>(widget))
   {
      if constexpr (std::is_integral_v<T>)
      {
         const std::optional<T> minimum = p->variable_->GetMinimum();
         const std::optional<T> maximum = p->variable_->GetMaximum();

         if (minimum.has_value())
         {
            slider->setMinimum(static_cast<int>(*minimum));
         }
         if (maximum.has_value())
         {
            slider->setMaximum(static_cast<int>(*maximum));
         }

         // If the slider is edited, stage a changed value
         QObject::connect(
            slider,
            &QSlider::valueChanged,
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
                  p->UpdateWidget(p->labelWidget_);
                  p->UpdateResetButton();
               }
               // If there is no staged value, or if the new value is different
               // than what is staged, attempt to stage the value
               else if (!staged.has_value() || static_cast<T>(i) != *staged)
               {
                  p->stagedValid_ = p->variable_->StageValue(static_cast<T>(i));
                  p->UpdateWidget(p->labelWidget_);
                  p->UpdateResetButton();
               }
               // Otherwise, don't process an unchanged value

               p->UpdateValidityDisplay();
            });
      }
   }

   p->UpdateWidgets();
}

template<class T>
void SettingsInterface<T>::SetLabelWidget(QWidget* widget)
{
   p->labelWidget_ = widget;
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
      auto sizePolicy = button->sizePolicy();
      sizePolicy.setRetainSizeWhenHidden(true);
      button->setSizePolicy(sizePolicy);

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
                             p->UpdateWidgets();
                             p->UpdateResetButton();
                          }
                          else
                          {
                             // Stage the default value
                             p->stagedValid_ =
                                p->variable_->StageValue(defaultValue);
                             p->UpdateWidgets();
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
   p->unitScale_        = scale;
   p->unitAbbreviation_ = abbreviation;
   p->unitEnabled_      = true;
   p->UpdateWidgets();
   p->UpdateUnitLabel();
}

template<class T>
void SettingsInterface<T>::EnableTrimming(bool trimmingEnabled)
{
   p->trimmingEnabled_ = trimmingEnabled;
}

template<class T>
void SettingsInterface<T>::SetInvalidTooltip(
   const std::optional<std::string>& tooltip)
{
   p->invalidTooltip_ = std::move(tooltip);
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
void SettingsInterface<T>::Impl::UpdateWidgets()
{
   UpdateWidget(editWidget_);
   UpdateWidget(labelWidget_);
}

template<class T>
void SettingsInterface<T>::Impl::UpdateWidget(QWidget* widget)
{
   if (widget == nullptr)
   {
      return;
   }

   // Use the staged value if present, otherwise the current value
   const std::optional<T> staged       = variable_->GetStaged();
   const T                value        = variable_->GetValue();
   const T&               currentValue = staged.has_value() ? *staged : value;

   if (auto* hotkeyEdit = dynamic_cast<ui::HotkeyEdit*>(widget))
   {
      if constexpr (std::is_same_v<T, std::string>)
      {
         QKeySequence keySequence =
            QKeySequence::fromString(QString::fromStdString(currentValue));
         hotkeyEdit->set_key_sequence(keySequence);
      }
   }
   else if (auto* lineEdit = dynamic_cast<QLineEdit*>(widget))
   {
      SetWidgetText(lineEdit, currentValue);
   }
   else if (auto* label = dynamic_cast<QLabel*>(widget))
   {
      SetWidgetText(label, currentValue);
   }
   else if (auto* checkBox = dynamic_cast<QCheckBox*>(widget))
   {
      if constexpr (std::is_same_v<T, bool>)
      {
         checkBox->setChecked(currentValue);
      }
   }
   else if (auto* comboBox = dynamic_cast<QComboBox*>(widget))
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
   else if (auto* spinBox = dynamic_cast<QSpinBox*>(widget))
   {
      if constexpr (std::is_integral_v<T>)
      {
         spinBox->setValue(static_cast<int>(currentValue));
      }
   }
   else if (auto* doubleSpinBox = dynamic_cast<QDoubleSpinBox*>(widget))
   {
      if constexpr (std::is_floating_point_v<T>)
      {
         auto doubleValue = static_cast<double>(currentValue);
         if (unitEnabled_)
         {
            doubleValue = doubleValue * unitScale_;
         }
         doubleSpinBox->setValue(doubleValue);
      }
   }
   else if (auto* slider = dynamic_cast<QSlider*>(widget))
   {
      if constexpr (std::is_integral_v<T>)
      {
         slider->setValue(static_cast<int>(currentValue));
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

   unitLabel_->setText(QString::fromStdString(unitAbbreviation_.value_or("")));
}

template<class T>
void SettingsInterface<T>::Impl::UpdateValidityDisplay()
{
   editWidget_->setStyleSheet(stagedValid_ ? kValidStyleSheet_ :
                                             kInvalidStyleSheet_);
   editWidget_->setToolTip(
      invalidTooltip_ && !stagedValid_ ? invalidTooltip_->c_str() : "");
}

template<class T>
void SettingsInterface<T>::Impl::UpdateResetButton()
{
   if (resetButton_ != nullptr)
   {
      resetButton_->setVisible(!self_->IsDefault());
   }
}

template class SettingsInterface<bool>;
template class SettingsInterface<double>;
template class SettingsInterface<std::int64_t>;
template class SettingsInterface<std::string>;

// Containers are not to be used directly
template class SettingsInterface<std::vector<std::int64_t>>;

} // namespace scwx::qt::settings
