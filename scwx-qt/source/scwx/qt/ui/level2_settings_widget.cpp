#include <scwx/qt/ui/level2_settings_widget.hpp>
#include <scwx/qt/ui/flow_layout.hpp>
#include <scwx/common/characters.hpp>

#include <execution>

#include <QCheckBox>
#include <QEvent>
#include <QGroupBox>
#include <QToolButton>

namespace scwx
{
namespace qt
{
namespace ui
{

class Level2SettingsWidgetImpl : public QObject
{
   Q_OBJECT

public:
   explicit Level2SettingsWidgetImpl(Level2SettingsWidget* self) :
       self_ {self},
       layout_ {new QVBoxLayout(self)},
       elevationGroupBox_ {},
       elevationButtons_ {},
       elevationCuts_ {},
       elevationButtonsChanged_ {false},
       resizeElevationButtons_ {false},
       settingsGroupBox_ {},
       declutterCheckBox_ {}
   {
      layout_->setContentsMargins(0, 0, 0, 0);

      elevationGroupBox_ = new QGroupBox(tr("Elevation"), self);
      new ui::FlowLayout(elevationGroupBox_);
      layout_->addWidget(elevationGroupBox_);

      settingsGroupBox_       = new QGroupBox(tr("Settings"), self);
      QLayout* settingsLayout = new QVBoxLayout(settingsGroupBox_);
      layout_->addWidget(settingsGroupBox_);

      declutterCheckBox_ = new QCheckBox(tr("Declutter"), settingsGroupBox_);
      settingsLayout->addWidget(declutterCheckBox_);

      settingsGroupBox_->setVisible(false);
   }
   ~Level2SettingsWidgetImpl() = default;

   void NormalizeElevationButtons();
   void SelectElevation(float elevation);
   void UpdateSettings();

   Level2SettingsWidget* self_;
   QLayout*              layout_;

   QGroupBox*              elevationGroupBox_;
   std::list<QToolButton*> elevationButtons_;
   std::vector<float>      elevationCuts_;
   bool                    elevationButtonsChanged_;
   bool                    resizeElevationButtons_;

   QGroupBox* settingsGroupBox_;
   QCheckBox* declutterCheckBox_;
};

Level2SettingsWidget::Level2SettingsWidget(QWidget* parent) :
    QWidget(parent), p {std::make_shared<Level2SettingsWidgetImpl>(this)}
{
}

Level2SettingsWidget::~Level2SettingsWidget() {}

bool Level2SettingsWidget::event(QEvent* event)
{
   if (event->type() == QEvent::Type::Paint)
   {
      if (p->elevationButtonsChanged_)
      {
         p->elevationButtonsChanged_ = false;
      }
      else if (p->resizeElevationButtons_)
      {
         p->NormalizeElevationButtons();
      }
   }

   return QWidget::event(event);
}

void Level2SettingsWidget::showEvent(QShowEvent* event)
{
   QWidget::showEvent(event);

   p->NormalizeElevationButtons();
}

void Level2SettingsWidgetImpl::NormalizeElevationButtons()
{
   // Set each elevation cut's tool button to the same size
   int elevationCutMaxWidth = 0;
   std::for_each(elevationButtons_.cbegin(),
                 elevationButtons_.cend(),
                 [&](auto& toolButton)
                 {
                    if (toolButton->isVisible())
                    {
                       elevationCutMaxWidth =
                          std::max(elevationCutMaxWidth, toolButton->width());
                    }
                 });

   if (elevationCutMaxWidth > 0)
   {
      std::for_each(elevationButtons_.cbegin(),
                    elevationButtons_.cend(),
                    [&](auto& toolButton)
                    { toolButton->setMinimumWidth(elevationCutMaxWidth); });

      resizeElevationButtons_ = false;
   }
}

void Level2SettingsWidgetImpl::SelectElevation(float elevation)
{
   self_->UpdateElevationSelection(elevation);

   emit self_->ElevationSelected(elevation);
}

void Level2SettingsWidget::UpdateElevationSelection(float elevation)
{
   QString buttonText {QString::number(elevation, 'f', 1) +
                       common::Characters::DEGREE};

   std::for_each(std::execution::par_unseq,
                 p->elevationButtons_.cbegin(),
                 p->elevationButtons_.cend(),
                 [&](auto& toolButton)
                 {
                    if (toolButton->text() == buttonText)
                    {
                       toolButton->setCheckable(true);
                       toolButton->setChecked(true);
                    }
                    else
                    {
                       toolButton->setChecked(false);
                       toolButton->setCheckable(false);
                    }
                 });
}

void Level2SettingsWidget::UpdateSettings(map::MapWidget* activeMap)
{
   float              currentElevation = activeMap->GetElevation();
   std::vector<float> elevationCuts    = activeMap->GetElevationCuts();

   if (p->elevationCuts_ != elevationCuts)
   {
      for (auto it = p->elevationButtons_.begin();
           it != p->elevationButtons_.end();)
      {
         delete *it;
         it = p->elevationButtons_.erase(it);
      }

      QLayout* layout = p->elevationGroupBox_->layout();

      // Create elevation cut tool buttons
      for (float elevationCut : elevationCuts)
      {
         QToolButton* toolButton = new QToolButton();
         toolButton->setText(QString::number(elevationCut, 'f', 1) +
                             common::Characters::DEGREE);
         layout->addWidget(toolButton);
         p->elevationButtons_.push_back(toolButton);

         connect(toolButton,
                 &QToolButton::clicked,
                 this,
                 [=]() { p->SelectElevation(elevationCut); });
      }

      p->elevationCuts_           = elevationCuts;
      p->elevationButtonsChanged_ = true;
      p->resizeElevationButtons_  = true;
   }

   UpdateElevationSelection(currentElevation);
}

} // namespace ui
} // namespace qt
} // namespace scwx

#include "level2_settings_widget.moc"
