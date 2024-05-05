#include <scwx/qt/ui/settings/unit_settings_widget.hpp>
#include <scwx/qt/settings/settings_interface.hpp>
#include <scwx/qt/settings/unit_settings.hpp>
#include <scwx/qt/types/unit_types.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include <QGridLayout>
#include <QLabel>
#include <QComboBox>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

namespace scwx
{
namespace qt
{
namespace ui
{

static const std::string logPrefix_ =
   "scwx::qt::ui::settings::unit_settings_widget";

class UnitSettingsWidget::Impl
{
public:
   explicit Impl(UnitSettingsWidget* self)
   {
      auto& unitSettings = settings::UnitSettings::Instance();

      gridLayout_ = new QGridLayout(self);
      contents_   = new QWidget(self);
      contents_->setLayout(gridLayout_);

      scrollArea_ = new QScrollArea(self);
      scrollArea_->setHorizontalScrollBarPolicy(
         Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
      scrollArea_->setWidgetResizable(true);
      scrollArea_->setWidget(contents_);

      layout_ = new QVBoxLayout(self);
      layout_->setContentsMargins(0, 0, 0, 0);
      layout_->addWidget(scrollArea_);

      self->setLayout(layout_);

      int row = 0;

      auto AddRow =
         [&row, &self, this](
            settings::SettingsInterface<std::string>& settingsInterface,
            const std::string&                        labelName,
            QComboBox*                                comboBox)
      {
         QLabel*      label = new QLabel(QObject::tr(labelName.c_str()), self);
         QToolButton* resetButton = new QToolButton(self);

         resetButton->setIcon(
            QIcon {":/res/icons/font-awesome-6/rotate-left-solid.svg"});
         resetButton->setVisible(false);

         settingsInterface.SetResetButton(resetButton);

         gridLayout_->addWidget(label, row, 0);
         gridLayout_->addWidget(comboBox, row, 1);
         gridLayout_->addWidget(resetButton, row, 2);

         // Add to settings list
         self->AddSettingsInterface(&settingsInterface);

         ++row;
      };

      QComboBox* accumulationComboBox = new QComboBox(self);
      accumulationComboBox->setSizePolicy(QSizePolicy::Expanding,
                                          QSizePolicy::Preferred);
      accumulationUnits_.SetSettingsVariable(unitSettings.accumulation_units());
      SCWX_SETTINGS_COMBO_BOX(accumulationUnits_,
                              accumulationComboBox,
                              types::AccumulationUnitsIterator(),
                              types::GetAccumulationUnitsName);
      AddRow(accumulationUnits_, "Accumulation", accumulationComboBox);

      QComboBox* echoTopsComboBox = new QComboBox(self);
      echoTopsComboBox->setSizePolicy(QSizePolicy::Expanding,
                                      QSizePolicy::Preferred);
      echoTopsUnits_.SetSettingsVariable(unitSettings.echo_tops_units());
      SCWX_SETTINGS_COMBO_BOX(echoTopsUnits_,
                              echoTopsComboBox,
                              types::EchoTopsUnitsIterator(),
                              types::GetEchoTopsUnitsName);
      AddRow(echoTopsUnits_, "Echo Tops", echoTopsComboBox);

      QComboBox* speedComboBox = new QComboBox(self);
      speedComboBox->setSizePolicy(QSizePolicy::Expanding,
                                   QSizePolicy::Preferred);
      speedUnits_.SetSettingsVariable(unitSettings.speed_units());
      SCWX_SETTINGS_COMBO_BOX(speedUnits_,
                              speedComboBox,
                              types::SpeedUnitsIterator(),
                              types::GetSpeedUnitsName);
      AddRow(speedUnits_, "Speed", speedComboBox);

      QComboBox* otherComboBox = new QComboBox(self);
      otherComboBox->setSizePolicy(QSizePolicy::Expanding,
                                   QSizePolicy::Preferred);
      otherUnits_.SetSettingsVariable(unitSettings.other_units());
      SCWX_SETTINGS_COMBO_BOX(otherUnits_,
                              otherComboBox,
                              types::OtherUnitsIterator(),
                              types::GetOtherUnitsName);
      AddRow(otherUnits_, "Other", otherComboBox);

      QSpacerItem* spacer =
         new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
      gridLayout_->addItem(spacer, row, 0);
   }
   ~Impl() = default;

   QWidget*     contents_;
   QLayout*     layout_;
   QScrollArea* scrollArea_ {};
   QGridLayout* gridLayout_ {};

   settings::SettingsInterface<std::string> accumulationUnits_ {};
   settings::SettingsInterface<std::string> echoTopsUnits_ {};
   settings::SettingsInterface<std::string> otherUnits_ {};
   settings::SettingsInterface<std::string> speedUnits_ {};
};

UnitSettingsWidget::UnitSettingsWidget(QWidget* parent) :
    SettingsPageWidget(parent), p {std::make_shared<Impl>(this)}
{
}

UnitSettingsWidget::~UnitSettingsWidget() = default;

} // namespace ui
} // namespace qt
} // namespace scwx
