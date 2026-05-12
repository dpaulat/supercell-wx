#include <scwx/qt/ui/sounding_parameters_widget.hpp>

#include <QGridLayout>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QFontDatabase>

namespace scwx::qt::ui
{

class SoundingParametersWidget::Impl
{
public:
   explicit Impl(SoundingParametersWidget* self) : self_(self)
   {
      fixedFont_ = QFontDatabase::systemFont(QFontDatabase::FixedFont);
   }
   ~Impl() = default;

   void SetupUi()
   {
      auto* mainLayout = new QVBoxLayout(self_);
      mainLayout->setContentsMargins(0, 0, 0, 0);

      auto* scrollArea = new QScrollArea();
      scrollArea->setWidgetResizable(true);
      scrollArea->setFrameShape(QFrame::NoFrame);

      auto* container = new QWidget();
      layout_         = new QGridLayout(container);
      layout_->setContentsMargins(4, 4, 4, 4);
      layout_->setSpacing(4);

      scrollArea->setWidget(container);
      mainLayout->addWidget(scrollArea);

      // Create labels for all parameters
      int row = 0;
      AddGroup("Convective", row++);
      sbcape_ = AddParam("SB CAPE",
                         "J/kg",
                         row++,
                         "Surface-Based Convective Available Potential Energy");
      sbcin_  = AddParam(
         "SB CIN", "J/kg", row++, "Surface-Based Convective Inhibition");
      sblcl_ = AddParam(
         "SB LCL", "hPa", row++, "Surface-Based Lifting Condensation Level");

      mlcape_ = AddParam("ML CAPE",
                         "J/kg",
                         row++,
                         "Mean-Layer Convective Available Potential Energy");
      mlcin_ =
         AddParam("ML CIN", "J/kg", row++, "Mean-Layer Convective Inhibition");
      mllcl_ = AddParam(
         "ML LCL", "hPa", row++, "Mean-Layer Lifting Condensation Level");

      mucape_ = AddParam("MU CAPE",
                         "J/kg",
                         row++,
                         "Most-Unstable Convective Available Potential Energy");
      mucin_  = AddParam(
         "MU CIN", "J/kg", row++, "Most-Unstable Convective Inhibition");
      mulcl_ = AddParam(
         "MU LCL", "hPa", row++, "Most-Unstable Lifting Condensation Level");

      AddGroup("Kinematic", row++);
      srh01_ = AddParam(
         "SRH 0-1km", "m2/s2", row++, "Storm-Relative Helicity (0-1 km)");
      srh03_ = AddParam(
         "SRH 0-3km", "m2/s2", row++, "Storm-Relative Helicity (0-3 km)");
      shear06_ = AddParam("Shear 0-6km", "m/s", row++, "Bulk Shear (0-6 km)");

      AddGroup("Thermodynamic", row++);
      lr75_ = AddParam("LR 700-500", "C/km", row++, "Lapse Rate (700-500 hPa)");
      pwat_ = AddParam("PWAT", "mm", row++, "Precipitable Water");

      AddGroup("Composite", row++);
      stp_ = AddParam(
         "STP", "", row++, "Significant Tornado Parameter (fixed layer)");
      scp_  = AddParam("SCP", "", row++, "Supercell Composite Parameter");
      ship_ = AddParam("SHIP", "", row++, "Significant Hail Parameter");
      ehi_  = AddParam("EHI", "", row++, "Energy Helicity Index");

      AddGroup("Hodograph Legend", row++);
      AddLegendItem("0-3 km", "#ff5500", row++);
      AddLegendItem("3-6 km", "#00cc00", row++);
      AddLegendItem("6-9 km", "#0066ff", row++);
      AddLegendItem("9-12 km", "#cc00cc", row++);

      layout_->setRowStretch(row, 1);
   }

   void AddLegendItem(const QString& text, const QString& color, int row)
   {
      auto* colorBox = new QFrame();
      colorBox->setFixedSize(12, 12);
      colorBox->setStyleSheet(
         QString("background-color: %1; border: 1px solid #333;").arg(color));
      layout_->addWidget(colorBox, row, 0, Qt::AlignRight | Qt::AlignVCenter);

      auto* label = new QLabel(text);
      label->setStyleSheet("color: #888; font-size: 9px;");
      layout_->addWidget(label, row, 1, 1, 2);
   }

   QLabel* AddGroup(const QString& name, int row)
   {
      auto* label = new QLabel(name);
      QFont font  = label->font();
      font.setBold(true);
      label->setFont(font);
      label->setStyleSheet(
         "color: #aaaaff; border-bottom: 1px solid #333; margin-top: 5px;");
      layout_->addWidget(label, row, 0, 1, 2);
      return label;
   }

   QLabel* AddParam(const QString& name,
                    const QString& unit,
                    int            row,
                    const QString& tooltip = "")
   {
      auto* nameLabel = new QLabel(name + ":");
      if (!tooltip.isEmpty())
      {
         nameLabel->setToolTip(tooltip);
      }
      layout_->addWidget(nameLabel, row, 0);

      auto* valLabel = new QLabel("--");
      valLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
      valLabel->setFont(fixedFont_);
      layout_->addWidget(valLabel, row, 1);

      if (!unit.isEmpty())
      {
         auto* unitLabel = new QLabel(unit);
         unitLabel->setStyleSheet("color: #888;");
         layout_->addWidget(unitLabel, row, 2);
      }
      return valLabel;
   }

   void Update(const std::shared_ptr<sounding::SoundingData>& sounding)
   {
      if (!sounding)
         return;

      auto setVal =
         [](QLabel* label, double val, int prec = 0, const QString& color = "")
      {
         label->setText(QString::number(val, 'f', prec));
         if (!color.isEmpty())
         {
            label->setStyleSheet(QString("color: %1;").arg(color));
         }
         else
         {
            label->setStyleSheet("");
         }
      };

      auto capeColor = [](double val) -> QString
      {
         static constexpr double kCapeExtreme = 2500.0;
         static constexpr double kCapeHigh    = 1000.0;
         static constexpr double kCapeLow     = 500.0;
         if (val >= kCapeExtreme)
            return "#ff5555"; // Red
         if (val >= kCapeHigh)
            return "#ffaa00"; // Orange
         if (val >= kCapeLow)
            return "#55ff55"; // Green
         return "";
      };

      auto cinColor = [](double val) -> QString
      {
         static constexpr double kCinModerate = 100.0;
         static constexpr double kCinLow      = 25.0;
         if (val >= kCinModerate)
            return "#ff5555"; // Red
         if (val >= kCinLow)
            return "#ffaa00"; // Orange
         return "";
      };

      auto srhColor = [](double val) -> QString
      {
         static constexpr double kSrhExtreme = 500.0;
         static constexpr double kSrhHigh    = 250.0;
         if (val >= kSrhExtreme)
            return "#ff55ff"; // Purple
         if (val >= kSrhHigh)
            return "#55aaff"; // Blue
         static constexpr double kSrhLow = 100.0;
         if (val >= kSrhLow)
            return "#55ff55"; // Green
         return "";
      };

      auto stpColor = [](double val) -> QString
      {
         static constexpr double kStpExtreme  = 4.0;
         static constexpr double kStpHigh     = 2.0;
         static constexpr double kStpModerate = 1.0;
         if (val >= kStpExtreme)
            return "#ff5555"; // Red
         if (val >= kStpHigh)
            return "#ffaa00"; // Orange
         if (val >= kStpModerate)
            return "#ffff55"; // Yellow
         return "";
      };

      setVal(
         sbcape_, sounding->sbcape_jkg(), 0, capeColor(sounding->sbcape_jkg()));
      setVal(sbcin_, sounding->sbcin_jkg(), 0, cinColor(sounding->sbcin_jkg()));
      setVal(sblcl_, sounding->lcl_pressure_hPa(), 0);

      setVal(
         mlcape_, sounding->mlcape_jkg(), 0, capeColor(sounding->mlcape_jkg()));
      setVal(mlcin_, sounding->mlcin_jkg(), 0, cinColor(sounding->mlcin_jkg()));
      setVal(mllcl_,
             sounding->lcl_pressure_hPa(),
             0); // Note: Should probably be MLLCL if available

      setVal(
         mucape_, sounding->mucape_jkg(), 0, capeColor(sounding->mucape_jkg()));
      setVal(mucin_, sounding->mucin_jkg(), 0, cinColor(sounding->mucin_jkg()));
      setVal(mulcl_,
             sounding->lcl_pressure_hPa(),
             0); // Note: Should probably be MULCL if available

      setVal(srh01_,
             sounding->storm_relative_helicity_m2s2(0, 1),
             0,
             srhColor(sounding->storm_relative_helicity_m2s2(0, 1)));
      setVal(srh03_,
             sounding->storm_relative_helicity_m2s2(0, 3),
             0,
             srhColor(sounding->storm_relative_helicity_m2s2(0, 3)));
      setVal(shear06_, sounding->bulk_shear_mps(0, 6), 1);

      setVal(lr75_, sounding->lapse_rate_c_km(7, 5), 1);
      setVal(pwat_, sounding->precipitable_water_mm(), 1);

      setVal(stp_,
             sounding->significant_tornado_parameter(),
             1,
             stpColor(sounding->significant_tornado_parameter()));
      setVal(scp_, sounding->supercell_composite_parameter(), 1);
      setVal(ship_, sounding->significant_hail_parameter(), 1);
      setVal(ehi_, sounding->energy_helicity_index(), 1);
   }

   SoundingParametersWidget* self_;
   QGridLayout*              layout_ {nullptr};
   QFont                     fixedFont_;

   QLabel* sbcape_ {nullptr};
   QLabel* sbcin_ {nullptr};
   QLabel* sblcl_ {nullptr};
   QLabel* mlcape_ {nullptr};
   QLabel* mlcin_ {nullptr};
   QLabel* mllcl_ {nullptr};
   QLabel* mucape_ {nullptr};
   QLabel* mucin_ {nullptr};
   QLabel* mulcl_ {nullptr};
   QLabel* srh01_ {nullptr};
   QLabel* srh03_ {nullptr};
   QLabel* shear06_ {nullptr};
   QLabel* lr75_ {nullptr};
   QLabel* pwat_ {nullptr};
   QLabel* stp_ {nullptr};
   QLabel* scp_ {nullptr};
   QLabel* ship_ {nullptr};
   QLabel* ehi_ {nullptr};
};

SoundingParametersWidget::SoundingParametersWidget(QWidget* parent) :
    QWidget(parent), p(std::make_unique<Impl>(this))
{
   p->SetupUi();
}

SoundingParametersWidget::~SoundingParametersWidget() = default;

void SoundingParametersWidget::SetSounding(
   const std::shared_ptr<sounding::SoundingData>& sounding)
{
   p->Update(sounding);
}

} // namespace scwx::qt::ui
