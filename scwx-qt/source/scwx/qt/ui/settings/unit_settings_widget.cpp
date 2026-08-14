#include <scwx/qt/ui/settings/unit_settings_widget.hpp>
#include <scwx/qt/settings/settings_interface.hpp>
#include <scwx/qt/settings/unit_settings.hpp>
#include <scwx/qt/types/unit_types.hpp>
#include <scwx/qt/ui/widgets/focused_combo_box.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include <QGridLayout>
#include <QLabel>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

namespace scwx::qt::ui
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
            QFocusedComboBox*                         comboBox)
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

      // Qt manages the memory for these widgets
      // NOLINTBEGIN(cppcoreguidelines-owning-memory)
      auto* accumulationComboBox = new QFocusedComboBox(self);
      accumulationComboBox->setSizePolicy(QSizePolicy::Expanding,
                                          QSizePolicy::Preferred);
      accumulationComboBox->setFocusPolicy(Qt::StrongFocus);
      accumulationUnits_.SetSettingsVariable(unitSettings.accumulation_units());
      SCWX_SETTINGS_COMBO_BOX(accumulationUnits_,
                              accumulationComboBox,
                              types::AccumulationUnitsIterator(),
                              types::GetAccumulationUnitsName);
      AddRow(accumulationUnits_, "Accumulation", accumulationComboBox);

      auto* echoTopsComboBox = new QFocusedComboBox(self);
      echoTopsComboBox->setSizePolicy(QSizePolicy::Expanding,
                                      QSizePolicy::Preferred);
      echoTopsComboBox->setFocusPolicy(Qt::StrongFocus);
      echoTopsUnits_.SetSettingsVariable(unitSettings.echo_tops_units());
      SCWX_SETTINGS_COMBO_BOX(echoTopsUnits_,
                              echoTopsComboBox,
                              types::EchoTopsUnitsIterator(),
                              types::GetEchoTopsUnitsName);
      AddRow(echoTopsUnits_, "Echo Tops", echoTopsComboBox);

      auto* radarBeamHeightComboBox = new QFocusedComboBox(self);
      radarBeamHeightComboBox->setSizePolicy(QSizePolicy::Expanding,
                                             QSizePolicy::Preferred);
      radarBeamHeightComboBox->setFocusPolicy(Qt::StrongFocus);
      radarBeamHeightComboBox->setToolTip(QObject::tr(
         "Reference for radar beam height in the Shift+hover tooltip. "
         "Above Radar Level (ARL) is the height above the radar antenna. "
         "Above Mean Sea Level (AMSL) includes the radar site elevation."));
      radarBeamHeightReference_.SetSettingsVariable(
         unitSettings.radar_beam_height_reference());
      SCWX_SETTINGS_COMBO_BOX(radarBeamHeightReference_,
                              radarBeamHeightComboBox,
                              types::RadarBeamHeightReferenceIterator(),
                              types::GetRadarBeamHeightReferenceName);
      AddRow(radarBeamHeightReference_,
             "Radar Beam Height",
             radarBeamHeightComboBox);

      auto* speedComboBox = new QFocusedComboBox(self);
      speedComboBox->setSizePolicy(QSizePolicy::Expanding,
                                   QSizePolicy::Preferred);
      speedComboBox->setFocusPolicy(Qt::StrongFocus);
      speedUnits_.SetSettingsVariable(unitSettings.speed_units());
      SCWX_SETTINGS_COMBO_BOX(speedUnits_,
                              speedComboBox,
                              types::SpeedUnitsIterator(),
                              types::GetSpeedUnitsName);
      AddRow(speedUnits_, "Speed", speedComboBox);

      auto* distanceComboBox = new QFocusedComboBox(self);
      distanceComboBox->setSizePolicy(QSizePolicy::Expanding,
                                      QSizePolicy::Preferred);
      distanceComboBox->setFocusPolicy(Qt::StrongFocus);
      distanceUnits_.SetSettingsVariable(unitSettings.distance_units());
      SCWX_SETTINGS_COMBO_BOX(distanceUnits_,
                              distanceComboBox,
                              types::DistanceUnitsIterator(),
                              types::GetDistanceUnitsName);
      AddRow(distanceUnits_, "Distance", distanceComboBox);

      auto* otherComboBox = new QFocusedComboBox(self);
      otherComboBox->setSizePolicy(QSizePolicy::Expanding,
                                   QSizePolicy::Preferred);
      otherComboBox->setFocusPolicy(Qt::StrongFocus);
      otherUnits_.SetSettingsVariable(unitSettings.other_units());
      SCWX_SETTINGS_COMBO_BOX(otherUnits_,
                              otherComboBox,
                              types::OtherUnitsIterator(),
                              types::GetOtherUnitsName);
      AddRow(otherUnits_, "Other", otherComboBox);

      auto* spacer =
         new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
      gridLayout_->addItem(spacer, row, 0);

      // NOLINTEND(cppcoreguidelines-owning-memory)
   }
   ~Impl() = default;

   Impl(const Impl&)            = delete;
   Impl(Impl&&)                 = delete;
   Impl& operator=(const Impl&) = delete;
   Impl& operator=(Impl&&)      = delete;

   QWidget*     contents_;
   QLayout*     layout_;
   QScrollArea* scrollArea_ {};
   QGridLayout* gridLayout_ {};

   settings::SettingsInterface<std::string> accumulationUnits_ {};
   settings::SettingsInterface<std::string> echoTopsUnits_ {};
   settings::SettingsInterface<std::string> radarBeamHeightReference_ {};
   settings::SettingsInterface<std::string> otherUnits_ {};
   settings::SettingsInterface<std::string> speedUnits_ {};
   settings::SettingsInterface<std::string> distanceUnits_ {};
};

UnitSettingsWidget::UnitSettingsWidget(QWidget* parent) :
    SettingsPageWidget(parent), p {std::make_shared<Impl>(this)}
{
}

UnitSettingsWidget::~UnitSettingsWidget() = default;

} // namespace scwx::qt::ui
