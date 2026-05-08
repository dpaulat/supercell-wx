#include <scwx/common/geographic.hpp>
#include <scwx/qt/map/map_annotation_layer.hpp>
#include <scwx/qt/map/map_annotation_types.hpp>
#include <scwx/qt/map/map_widget.hpp>
#include <scwx/qt/settings/ui_settings.hpp>
#include <scwx/qt/settings/unit_settings.hpp>
#include <scwx/qt/types/unit_types.hpp>
#include <scwx/qt/ui/map_annotation_dock_widget.hpp>

#include <units/length.h>

#include <boost/uuid/uuid.hpp>
#include <boost/json.hpp>

#include <QButtonGroup>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QEvent>
#include <QFrame>
#include <QApplication>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QPointer>
#include <QSignalBlocker>
#include <QSlider>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QToolButton>
#include <QThread>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <array>
#include <optional>
#include <utility>

#include <QVariant>

// Tuned preset/layout values plus Qt parent ownership are intentional here.
// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,cppcoreguidelines-owning-memory,cppcoreguidelines-pro-bounds-constant-array-index,bugprone-narrowing-conversions,cppcoreguidelines-narrowing-conversions)
namespace scwx::qt::ui
{

namespace
{
/** Minimum stroke width on the map (~0.1 statute miles). */
constexpr double                     kStrokeWidthMinM {160.9344};
constexpr double                     kStrokeWidthMaxM {80467.2};
constexpr std::array<const char*, 5> kBrushPresetNames {
   {"Extra fine", "Fine", "Medium", "Heavy", "Very wide"}};
constexpr int kStrokeWidthSliderSteps {1000};

double MetersToDisplayDistance(double meters, types::DistanceUnits u)
{
   return meters * scwx::common::kKilometersPerMeter *
          types::GetDistanceUnitsScale(u);
}

double DisplayDistanceToMeters(double display, types::DistanceUnits u)
{
   const double s = types::GetDistanceUnitsScale(u);
   if (s == 0.0)
   {
      return display;
   }
   return display / (scwx::common::kKilometersPerMeter * s);
}

QString FormatBrushPresetDistance(double meters, types::DistanceUnits units)
{
   const double display = MetersToDisplayDistance(meters, units);
   std::string  abbrev  = types::GetDistanceUnitsAbbreviation(units);
   if (abbrev.empty())
   {
      abbrev = "user";
   }

   int decimals = 1;
   if (display < 1.0)
   {
      decimals = 2;
   }
   else if (display >= 10.0)
   {
      decimals = 0;
   }

   return QStringLiteral("%1 %2")
      .arg(QString::number(display, 'f', decimals))
      .arg(QString::fromStdString(abbrev));
}

std::array<double, 5> BrushPresetDiameterMeters(types::DistanceUnits units)
{
   if (units == types::DistanceUnits::Miles)
   {
      return {160.9344, 402.336, 804.672, 1609.344, 4828.032};
   }
   return {165.0, 250.0, 500.0, 1500.0, 4500.0};
}

int StrokeWidthSliderPositionFromMeters(double meters)
{
   const double clamped =
      std::clamp(meters, kStrokeWidthMinM, kStrokeWidthMaxM);
   const double logMin = std::log(kStrokeWidthMinM);
   const double logMax = std::log(kStrokeWidthMaxM);
   const double ratio  = (std::log(clamped) - logMin) / (logMax - logMin);
   return static_cast<int>(
      std::round(ratio * static_cast<double>(kStrokeWidthSliderSteps)));
}

double StrokeWidthMetersFromSliderPosition(int position)
{
   const int    clampedPos = std::clamp(position, 0, kStrokeWidthSliderSteps);
   const double ratio      = static_cast<double>(clampedPos) /
                        static_cast<double>(kStrokeWidthSliderSteps);
   const double logMin = std::log(kStrokeWidthMinM);
   const double logMax = std::log(kStrokeWidthMaxM);
   return std::exp(logMin + ratio * (logMax - logMin));
}

class BrushScaleLabelsWidget : public QWidget
{
public:
   struct LabelMark
   {
      double  ratio {0.0};
      QString text {};
   };

   explicit BrushScaleLabelsWidget(QSlider* slider, QWidget* parent = nullptr) :
       QWidget(parent), slider_ {slider}
   {
      if (slider_ != nullptr)
      {
         slider_->installEventFilter(this);
      }
   }

   void SetLabels(std::vector<LabelMark> labels)
   {
      labels_ = std::move(labels);
      update();
   }

   [[nodiscard]] QSize minimumSizeHint() const override
   {
      return QSize {160, fontMetrics().height() + 6};
   }

protected:
   bool eventFilter(QObject* watched, QEvent* event) override
   {
      if (watched == slider_ &&
          (event->type() == QEvent::Move || event->type() == QEvent::Resize ||
           event->type() == QEvent::StyleChange))
      {
         update();
      }
      return QWidget::eventFilter(watched, event);
   }

   void paintEvent(QPaintEvent* event) override
   {
      QWidget::paintEvent(event);
      if (slider_ == nullptr || labels_.empty())
      {
         return;
      }

      QStyleOptionSlider option;
      option.initFrom(slider_);
      option.subControls    = QStyle::SC_SliderGroove;
      option.orientation    = slider_->orientation();
      option.minimum        = slider_->minimum();
      option.maximum        = slider_->maximum();
      option.sliderPosition = slider_->sliderPosition();
      option.sliderValue    = slider_->value();
      option.tickPosition   = slider_->tickPosition();
      option.tickInterval   = slider_->tickInterval();
      const QRect groove    = slider_->style()->subControlRect(
         QStyle::CC_Slider, &option, QStyle::SC_SliderGroove, slider_);
      const QPoint grooveLeft =
         mapFromGlobal(slider_->mapToGlobal(groove.topLeft()));
      const QPoint grooveRight =
         mapFromGlobal(slider_->mapToGlobal(groove.topRight()));
      const int leftX  = grooveLeft.x();
      const int rightX = grooveRight.x();

      QPainter painter(this);
      painter.setPen(palette().color(QPalette::WindowText));
      const QFontMetrics fm = painter.fontMetrics();
      const int          y  = fm.ascent() + 1;
      for (const auto& label : labels_)
      {
         const int x =
            leftX + static_cast<int>(std::round(
                       label.ratio * static_cast<double>(rightX - leftX)));
         const int w     = fm.horizontalAdvance(label.text);
         const int drawX = std::clamp(x - w / 2, 0, std::max(0, width() - w));
         painter.drawText(drawX, y, label.text);
      }
   }

private:
   QSlider*               slider_ {nullptr};
   std::vector<LabelMark> labels_ {};
};

struct BrushScaleMark
{
   double value;
   int    decimals;
};

struct PersistedDockState
{
   int     toolId {static_cast<int>(map::MapAnnotationTool::None)};
   bool    drawingsVisible {true};
   int     brushPresetIndex {0};
   double  strokeWidthM {500.0};
   bool    fill {false};
   QString strokeColorHex {QStringLiteral("#e6ff3232")};
   bool    overlayVisible {true};
   bool    expanded {false};
   bool    floating {false};
   int     attachedX {-1};
   int     attachedY {-1};
   int     floatingX {-1};
   int     floatingY {-1};
   bool    floatingPositionGlobal {false};
   /** 0/1: legacy; 2+: floating_x/y are parent-relative (Qt::Tool parent). */
   int persistVersion {0};
};

constexpr int kMapAnnotationPersistVersion = 2;

PersistedDockState LoadDockState()
{
   PersistedDockState state;
   const std::string  serialized =
      settings::UiSettings::Instance().map_annotation_state().GetValue();
   if (serialized.empty())
   {
      return state;
   }

   boost::system::error_code ec;
   const auto                value = boost::json::parse(serialized, ec);
   if (ec || !value.is_object())
   {
      return state;
   }

   const auto& object = value.as_object();

   if (const auto* v = object.if_contains("persist_version");
       v != nullptr && v->is_int64())
   {
      state.persistVersion = static_cast<int>(v->as_int64());
   }
   if (const auto* v = object.if_contains("tool_id");
       v != nullptr && v->is_int64())
   {
      state.toolId = static_cast<int>(v->as_int64());
   }
   if (const auto* v = object.if_contains("drawings_visible");
       v != nullptr && v->is_bool())
   {
      state.drawingsVisible = v->as_bool();
   }
   if (const auto* v = object.if_contains("brush_preset_index");
       v != nullptr && v->is_int64())
   {
      state.brushPresetIndex = static_cast<int>(v->as_int64());
   }
   if (const auto* strokeWidthValue = object.if_contains("stroke_width_m");
       strokeWidthValue != nullptr)
   {
      if (strokeWidthValue->is_double())
      {
         state.strokeWidthM = strokeWidthValue->as_double();
      }
      else if (strokeWidthValue->is_int64())
      {
         state.strokeWidthM = static_cast<double>(strokeWidthValue->as_int64());
      }
   }
   if (const auto* v = object.if_contains("fill"); v != nullptr && v->is_bool())
   {
      state.fill = v->as_bool();
   }
   if (const auto* v = object.if_contains("stroke_color_hex");
       v != nullptr && v->is_string())
   {
      state.strokeColorHex =
         QString::fromStdString(std::string(v->as_string()));
   }
   if (const auto* v = object.if_contains("overlay_visible");
       v != nullptr && v->is_bool())
   {
      state.overlayVisible = v->as_bool();
   }
   if (const auto* v = object.if_contains("expanded");
       v != nullptr && v->is_bool())
   {
      state.expanded = v->as_bool();
   }
   if (const auto* v = object.if_contains("floating");
       v != nullptr && v->is_bool())
   {
      state.floating = v->as_bool();
   }
   if (const auto* v = object.if_contains("attached_x");
       v != nullptr && v->is_int64())
   {
      state.attachedX = static_cast<int>(v->as_int64());
   }
   if (const auto* v = object.if_contains("attached_y");
       v != nullptr && v->is_int64())
   {
      state.attachedY = static_cast<int>(v->as_int64());
   }
   if (const auto* v = object.if_contains("floating_x");
       v != nullptr && v->is_int64())
   {
      state.floatingX = static_cast<int>(v->as_int64());
   }
   if (const auto* v = object.if_contains("floating_y");
       v != nullptr && v->is_int64())
   {
      state.floatingY = static_cast<int>(v->as_int64());
   }
   if (const auto* v = object.if_contains("floating_position_global");
       v != nullptr && v->is_bool())
   {
      state.floatingPositionGlobal = v->as_bool();
   }

   return state;
}

std::vector<BrushScaleMark> BrushScaleMarks(types::DistanceUnits units)
{
   if (units == types::DistanceUnits::Miles)
   {
      return {{0.10, 2}, {5.0, 0}, {50.0, 0}};
   }
   return {{0.2, 1},
           {0.5, 1},
           {1.0, 1},
           {2.0, 0},
           {5.0, 0},
           {10.0, 0},
           {25.0, 0},
           {50.0, 0}};
}

QPoint ClampOverlayPosition(const QWidget* host,
                            const QWidget* overlay,
                            QPoint         position)
{
   if (host == nullptr || overlay == nullptr)
   {
      return position;
   }

   const int maxX = std::max(8, host->width() - overlay->width() - 8);
   const int maxY = std::max(8, host->height() - overlay->height() - 8);

   position.setX(std::clamp(position.x(), 8, maxX));
   position.setY(std::clamp(position.y(), 8, maxY));

   return position;
}
} // namespace

class MapAnnotationDockWidget::Impl
{
public:
   Impl(const Impl&)            = delete;
   Impl& operator=(const Impl&) = delete;
   Impl(Impl&&)                 = delete;
   Impl& operator=(Impl&&)      = delete;

   explicit Impl(MapAnnotationDockWidget* self) :
       self_ {self}, overlayParent_ {self->parentWidget()}
   {
   }
   ~Impl()
   {
      settings::UnitSettings::Instance()
         .distance_units()
         .UnregisterValueChangedCallback(distanceUnitsCallbackUuid_);
   }

   [[nodiscard]] std::vector<std::shared_ptr<map::MapAnnotationLayer>>
   LayersForToolStyle() const
   {
      if (getBroadcastLayers_)
      {
         return getBroadcastLayers_();
      }
      if (layer_ != nullptr)
      {
         return {layer_};
      }
      return {};
   }

   [[nodiscard]] map::MapAnnotationTool CurrentTool() const
   {
      const int id =
         (toolButtonGroup_ != nullptr) ? toolButtonGroup_->checkedId() : -1;
      return (id >= 0) ? static_cast<map::MapAnnotationTool>(id) :
                         map::MapAnnotationTool::None;
   }

   void DisconnectLayer()
   {
      for (const QMetaObject::Connection& c : connections_)
      {
         QObject::disconnect(c);
      }
      connections_.clear();
      layer_.reset();
   }

   void UpdatePlacement()
   {
      if (!overlayVisible_)
      {
         self_->hide();
         return;
      }

      if (self_->layout() != nullptr)
      {
         self_->layout()->activate();
      }
      if (expandedPanel_ != nullptr && expandedPanel_->layout() != nullptr)
      {
         expandedPanel_->layout()->activate();
         expandedPanel_->adjustSize();
      }
      self_->updateGeometry();
      self_->adjustSize();

      if (floating_)
      {
         if (floatingPosition_.has_value())
         {
            self_->move(*floatingPosition_);
         }
         self_->show();
         self_->raise();
         return;
      }

      if (hostMapWidget_ == nullptr)
      {
         return;
      }

      QPoint position = attachedPosition_.value_or(QPoint {
         std::max(8, (hostMapWidget_->width() - self_->width()) / 2), 10});
      position        = ClampOverlayPosition(hostMapWidget_, self_, position);
      if (attachedPosition_.has_value())
      {
         attachedPosition_ = position;
      }

      QWidget* const stableParent =
         !overlayParent_.isNull() ? overlayParent_.data() : nullptr;
      QWidget* const hostWindow = hostMapWidget_->window();
      const bool     hostIsInStableWindow =
         stableParent != nullptr && hostWindow == stableParent->window();

      QWidget* const overlayParent =
         hostIsInStableWindow ? stableParent : hostWindow;
      if (overlayParent == nullptr)
      {
         return;
      }
      if (self_->parentWidget() != overlayParent ||
          (self_->windowFlags() & Qt::Window) != 0)
      {
         self_->setParent(overlayParent, Qt::Widget);
      }
      self_->move(hostMapWidget_->mapTo(overlayParent, position));
      self_->show();
      self_->raise();
   }

   void SaveState() const
   {
      if (suppressPersist_)
      {
         return;
      }
      boost::json::object object;
      object["tool_id"]            = static_cast<std::int64_t>(CurrentTool());
      object["drawings_visible"]   = drawingsVisible_;
      object["brush_preset_index"] = static_cast<std::int64_t>(
         (brushPresetCombo_ != nullptr) ? brushPresetCombo_->currentIndex() :
                                          0);
      object["stroke_width_m"] = strokeWidthM_;
      object["fill"] = (fillCheck_ != nullptr) && fillCheck_->isChecked();
      object["stroke_color_hex"] =
         strokeColor_.name(QColor::HexArgb).toStdString();
      object["overlay_visible"] = overlayVisible_;
      object["expanded"]        = expanded_;
      object["floating"]        = floating_;
      object["attached_x"]      = static_cast<std::int64_t>(
         attachedPosition_.has_value() ? attachedPosition_->x() : -1);
      object["attached_y"] = static_cast<std::int64_t>(
         attachedPosition_.has_value() ? attachedPosition_->y() : -1);
      if (floating_ && self_->parentWidget() != nullptr)
      {
         const QPoint rel     = self_->pos();
         object["floating_x"] = static_cast<std::int64_t>(rel.x());
         object["floating_y"] = static_cast<std::int64_t>(rel.y());
         object["floating_position_global"] = false;
      }
      else
      {
         object["floating_x"] = static_cast<std::int64_t>(
            floatingPosition_.has_value() ? floatingPosition_->x() : -1);
         object["floating_y"] = static_cast<std::int64_t>(
            floatingPosition_.has_value() ? floatingPosition_->y() : -1);
         object["floating_position_global"] =
            floating_ && self_->parentWidget() == nullptr;
      }
      object["persist_version"] =
         static_cast<std::int64_t>(kMapAnnotationPersistVersion);
      static_cast<void>(
         settings::UiSettings::Instance().map_annotation_state().StageValue(
            boost::json::serialize(object)));
   }

   void LoadState()
   {
      const PersistedDockState state = LoadDockState();
      suppressPersist_               = true;

      strokeWidthM_ =
         std::clamp(state.strokeWidthM, kStrokeWidthMinM, kStrokeWidthMaxM);
      strokeColor_ = QColor {state.strokeColorHex};
      if (!strokeColor_.isValid())
      {
         strokeColor_ = QColor {255, 50, 50, 230};
      }
      // Draw toolbar is always available; legacy overlay_visible=false only hid
      // the View menu control.
      overlayVisible_         = true;
      expanded_               = state.expanded;
      const bool shouldFloat  = state.floating;
      floating_               = false;
      pendingRestoreFloating_ = shouldFloat;
      legacyGlobalFloatingPos_ =
         shouldFloat && (state.persistVersion < kMapAnnotationPersistVersion);

      if (state.attachedX >= 0 && state.attachedY >= 0)
      {
         attachedPosition_ = QPoint {state.attachedX, state.attachedY};
      }
      else
      {
         attachedPosition_.reset();
      }

      if (state.floatingX >= 0 && state.floatingY >= 0)
      {
         floatingPosition_       = QPoint {state.floatingX, state.floatingY};
         floatingPositionGlobal_ = state.floatingPositionGlobal;
      }
      else
      {
         floatingPosition_.reset();
         floatingPositionGlobal_ = false;
      }

      if (colorButton_ != nullptr)
      {
         colorButton_->setStyleSheet(
            QStringLiteral("background-color: %1")
               .arg(strokeColor_.name(QColor::HexArgb)));
      }
      if (fillCheck_ != nullptr)
      {
         const QSignalBlocker blocker {fillCheck_};
         fillCheck_->setChecked(state.fill);
      }
      UpdateCustomBrushSlider();
      UpdateCustomBrushValueLabel();

      if (brushPresetCombo_ != nullptr)
      {
         const int index = std::clamp(
            state.brushPresetIndex, 0, brushPresetCombo_->count() - 1);
         const QSignalBlocker blocker {brushPresetCombo_};
         brushPresetCombo_->setCurrentIndex(index);
      }
      UpdateBrushControlVisibility();

      if (toolButtonGroup_ != nullptr)
      {
         if (auto* button = toolButtonGroup_->button(state.toolId);
             button != nullptr)
         {
            const QSignalBlocker blocker {toolButtonGroup_};
            button->setChecked(true);
         }
      }
      drawingsVisible_ = state.drawingsVisible;
      if (visibilityButton_ != nullptr)
      {
         const QSignalBlocker blocker {visibilityButton_};
         visibilityButton_->setChecked(drawingsVisible_);
         visibilityButton_->setText(drawingsVisible_ ?
                                       MapAnnotationDockWidget::tr("Hide") :
                                       MapAnnotationDockWidget::tr("Show"));
      }
      UpdateFillVisibility();
      UpdateFloatButtonText();
      SetExpanded(expanded_);
      SetOverlayVisible(overlayVisible_);
      suppressPersist_ = false;
   }

   void SetExpanded(bool expanded)
   {
      if (floating_ && !expanded)
      {
         expanded = true;
      }
      expanded_ = expanded;
      if (collapsedButton_ != nullptr)
      {
         collapsedButton_->setVisible(false);
      }
      if (expandedPanel_ != nullptr)
      {
         expandedPanel_->setVisible(overlayVisible_ && expanded_);
      }
      if (minimizeButton_ != nullptr)
      {
         minimizeButton_->setVisible(!floating_);
      }
      UpdatePlacement();
      SaveState();
   }

   void SetOverlayVisible(bool visible)
   {
      overlayVisible_ = visible;
      if (!overlayVisible_)
      {
         // Turning the whole overlay off: reset tools so the map does not keep
         // intercepting drags.
         constexpr auto kOffTool = map::MapAnnotationTool::None;
         for (const auto& L : LayersForToolStyle())
         {
            if (L != nullptr)
            {
               L->ClearAll();
               L->SetTool(kOffTool);
            }
         }
         UpdateToolButtons(kOffTool);
         self_->hide();
         SaveState();
         return;
      }
      SetExpanded(expanded_);
      UpdatePlacement();
      SaveState();
   }

   void SetFloating(bool floating)
   {
      if (floating_ == floating)
      {
         return;
      }

      if (floating)
      {
         const QPoint   globalPosition = self_->mapToGlobal(QPoint {0, 0});
         QWidget* const stableParent =
            !overlayParent_.isNull() ? overlayParent_.data() : nullptr;
         QWidget* const hostWindow =
            (hostMapWidget_ != nullptr) ? hostMapWidget_->window() : nullptr;
         const bool hostIsInStableWindow =
            stableParent != nullptr && hostWindow == stableParent->window();
         QWidget* const ownerWindow =
            hostIsInStableWindow ? stableParent->window() : nullptr;
         const QWidget* const previousParent = self_->parentWidget();
         self_->hide();
         self_->setParent(ownerWindow,
                          Qt::Tool | Qt::CustomizeWindowHint |
                             Qt::WindowTitleHint);
         floating_ = true;
         expanded_ = true;
         QPoint floatPos;
         if (floatingPosition_.has_value())
         {
            floatPos = *floatingPosition_;
            if ((legacyGlobalFloatingPos_ || floatingPositionGlobal_) &&
                ownerWindow != nullptr)
            {
               floatPos                 = ownerWindow->mapFromGlobal(floatPos);
               legacyGlobalFloatingPos_ = false;
               floatingPositionGlobal_  = false;
            }
            else if (!floatingPositionGlobal_ && ownerWindow == nullptr)
            {
               if (previousParent != nullptr)
               {
                  floatPos = previousParent->mapToGlobal(floatPos);
               }
               else
               {
                  floatPos = globalPosition;
               }
               floatingPositionGlobal_ = true;
            }
         }
         else if (ownerWindow != nullptr)
         {
            floatPos = ownerWindow->mapFromGlobal(globalPosition);
            floatingPositionGlobal_ = false;
         }
         else
         {
            floatPos                = globalPosition;
            floatingPositionGlobal_ = true;
         }
         floatingPosition_ = floatPos;
         self_->move(floatPos);
         SetExpanded(true);
         if (overlayVisible_)
         {
            self_->show();
         }
      }
      else
      {
         const QPoint globalTopLeft = self_->mapToGlobal(QPoint {0, 0});
         self_->hide();
         floating_ = false;
         if (hostMapWidget_ == nullptr && floatingDockHostResolver_)
         {
            if (QWidget* const resolved = floatingDockHostResolver_())
            {
               self_->AttachToMap(resolved);
               if (auto* const mw =
                      qobject_cast<map::MapWidget*>(hostMapWidget_.data()))
               {
                  self_->BindToLayer(mw->map_annotation_layer(), false);
               }
            }
         }
         if (hostMapWidget_ != nullptr)
         {
            attachedPosition_ = ClampOverlayPosition(
               hostMapWidget_,
               self_,
               hostMapWidget_->mapFromGlobal(globalTopLeft));
         }
         else
         {
            QWidget* const owner = self_->parentWidget();
            self_->setParent(owner != self_ ? owner : nullptr, Qt::Widget);
            attachedPosition_.reset();
         }
         floatingPosition_.reset();
         floatingPositionGlobal_ = false;
         UpdatePlacement();
      }

      UpdateFloatButtonText();
      if (minimizeButton_ != nullptr)
      {
         minimizeButton_->setVisible(!floating_);
      }
      SaveState();
   }

   void UpdateFloatButtonText()
   {
      if (floatButton_ != nullptr)
      {
         floatButton_->setText(floating_ ?
                                  MapAnnotationDockWidget::tr("Dock") :
                                  MapAnnotationDockWidget::tr("Pop-Out"));
      }
   }

   void UpdateToolButtons(map::MapAnnotationTool tool)
   {
      if (toolButtonGroup_ == nullptr)
      {
         return;
      }
      if (auto* button = toolButtonGroup_->button(static_cast<int>(tool));
          button != nullptr)
      {
         const QSignalBlocker blocker {toolButtonGroup_};
         button->setChecked(true);
      }
      UpdateFillVisibility();
   }

   void PushStyleFromUi()
   {
      map::MapAnnotationStyle st {};
      if (layer_ != nullptr)
      {
         st = layer_->style();
      }
      st.strokeWidthM = units::length::meters<double> {
         std::clamp(strokeWidthM_, kStrokeWidthMinM, kStrokeWidthMaxM)};
      st.strokeColor = {static_cast<float>(strokeColor_.redF()),
                        static_cast<float>(strokeColor_.greenF()),
                        static_cast<float>(strokeColor_.blueF()),
                        static_cast<float>(strokeColor_.alphaF())};
      st.polygonFill = (fillCheck_ != nullptr) && fillCheck_->isChecked();
      st.hatchFill   = false;
      st.strokeStyle = map::MapAnnotationStrokeStyle::Solid;

      for (const auto& L : LayersForToolStyle())
      {
         if (L != nullptr)
         {
            L->SetVisible(drawingsVisible_);
            L->SetStyle(st);
         }
      }
   }

   void PullStyleToUi()
   {
      if (layer_ == nullptr)
      {
         return;
      }

      const auto st = layer_->style();
      strokeWidthM_ = std::clamp(
         st.strokeWidthM.value(), kStrokeWidthMinM, kStrokeWidthMaxM);
      strokeColor_.setRgbF(static_cast<double>(st.strokeColor[0]),
                           static_cast<double>(st.strokeColor[1]),
                           static_cast<double>(st.strokeColor[2]),
                           static_cast<double>(st.strokeColor[3]));
      if (colorButton_ != nullptr)
      {
         colorButton_->setStyleSheet(
            QStringLiteral("background-color: %1")
               .arg(strokeColor_.name(QColor::HexArgb)));
      }
      if (fillCheck_ != nullptr)
      {
         fillCheck_->setChecked(st.polygonFill);
      }
      UpdateCustomBrushSlider();
      UpdateCustomBrushValueLabel();
      UpdateBrushPresetUiFromStrokeMeters(strokeWidthM_);
      UpdateToolButtons(layer_->tool());
   }

   void SyncDistanceSpinBoxesToSettings()
   {
      const std::string newName =
         settings::UnitSettings::Instance().distance_units().GetValue();
      const types::DistanceUnits newUnits =
         types::GetDistanceUnitsFromName(newName);
      strokeWidthM_ =
         std::clamp(strokeWidthM_, kStrokeWidthMinM, kStrokeWidthMaxM);
      UpdateBrushPresetLabels(newUnits);
      UpdateCustomBrushMarks(newUnits);
      UpdateCustomBrushValueLabel();
      UpdateBrushPresetUiFromStrokeMeters(strokeWidthM_);
      lastDistanceUnitsName_ = newName;
   }

   void OnUserDistanceUnitsChanged(const std::string& /*newValue*/)
   {
      SyncDistanceSpinBoxesToSettings();
      PushStyleFromUi();
   }

   void UpdateBrushPresetLabels(types::DistanceUnits units)
   {
      if (brushPresetCombo_ == nullptr)
      {
         return;
      }
      const auto           presetMeters = BrushPresetDiameterMeters(units);
      const QSignalBlocker blocker {brushPresetCombo_};
      brushPresetCombo_->setItemText(0, MapAnnotationDockWidget::tr("Custom"));
      for (std::size_t i = 0; i < presetMeters.size(); ++i)
      {
         brushPresetCombo_->setItemText(
            static_cast<int>(i) + 1,
            MapAnnotationDockWidget::tr("%1 (%2)")
               .arg(MapAnnotationDockWidget::tr(kBrushPresetNames[i]))
               .arg(FormatBrushPresetDistance(presetMeters[i], units)));
         brushPresetCombo_->setItemData(static_cast<int>(i) + 1,
                                        presetMeters[i]);
      }
   }

   void UpdateBrushPresetUiFromStrokeMeters(double strokeWidthMeters)
   {
      if (brushPresetCombo_ == nullptr)
      {
         return;
      }
      const auto units = types::GetDistanceUnitsFromName(
         settings::UnitSettings::Instance().distance_units().GetValue());
      const auto presetMeters = BrushPresetDiameterMeters(units);
      int        match        = 0;
      for (std::size_t i = 0; i < presetMeters.size(); ++i)
      {
         const double tol = std::max(10.0, 0.08 * presetMeters[i]);
         if (std::abs(strokeWidthMeters - presetMeters[i]) <= tol)
         {
            match = static_cast<int>(i) + 1;
            break;
         }
      }
      const QSignalBlocker blocker {brushPresetCombo_};
      brushPresetCombo_->setCurrentIndex(match);
      UpdateBrushControlVisibility();
   }

   void UpdateBrushControlVisibility()
   {
      const bool showCustom = (brushPresetCombo_ == nullptr) ||
                              brushPresetCombo_->currentIndex() <= 0;
      if (customBrushWidget_ != nullptr)
      {
         customBrushWidget_->setVisible(showCustom);
      }
      UpdatePlacement();
   }

   void UpdateFillVisibility()
   {
      if (fillCheck_ == nullptr)
      {
         return;
      }
      const auto tool = CurrentTool();
      fillCheck_->setVisible(tool == map::MapAnnotationTool::Circle ||
                             tool == map::MapAnnotationTool::Rectangle);
      UpdatePlacement();
   }

   void UpdateCustomBrushSlider()
   {
      if (strokeWidthSlider_ == nullptr)
      {
         return;
      }
      const QSignalBlocker blocker {strokeWidthSlider_};
      strokeWidthSlider_->setValue(
         StrokeWidthSliderPositionFromMeters(strokeWidthM_));
   }

   void UpdateCustomBrushValueLabel()
   {
      if (customBrushValueLabel_ == nullptr)
      {
         return;
      }
      const auto units = types::GetDistanceUnitsFromName(
         settings::UnitSettings::Instance().distance_units().GetValue());
      customBrushValueLabel_->setText(
         MapAnnotationDockWidget::tr("Custom width: %1")
            .arg(FormatBrushPresetDistance(strokeWidthM_, units)));
   }

   void UpdateCustomBrushMarks(types::DistanceUnits units)
   {
      if (customBrushMarksWidget_ == nullptr)
      {
         return;
      }
      std::vector<BrushScaleLabelsWidget::LabelMark> labels;
      const auto marks = BrushScaleMarks(units);
      labels.reserve(marks.size());
      std::string abbrev = types::GetDistanceUnitsAbbreviation(units);
      if (abbrev.empty())
      {
         abbrev = "user";
      }
      for (const auto& mark : marks)
      {
         const int pos = StrokeWidthSliderPositionFromMeters(
            DisplayDistanceToMeters(mark.value, units));
         labels.push_back(BrushScaleLabelsWidget::LabelMark {
            .ratio = static_cast<double>(pos) /
                     static_cast<double>(kStrokeWidthSliderSteps),
            .text = QStringLiteral("%1 %2")
                       .arg(QString::number(mark.value, 'f', mark.decimals))
                       .arg(QString::fromStdString(abbrev)),
         });
      }
      customBrushMarksWidget_->SetLabels(std::move(labels));
   }

   MapAnnotationDockWidget* self_ {nullptr};
   QPointer<QWidget>        overlayParent_ {};
   QPointer<QWidget>        hostMapWidget_ {};
   bool                     expanded_ {false};
   bool                     overlayVisible_ {true};
   bool                     floating_ {false};
   bool                     dragging_ {false};
   QPoint                   dragStartGlobal_ {};
   QPoint                   dragStartPosition_ {};
   QPoint                   dragStartOverlayGlobal_ {};
   std::optional<QPoint>    attachedPosition_ {};
   std::optional<QPoint>    floatingPosition_ {};
   bool                     floatingPositionGlobal_ {false};

   QPushButton*                             collapsedButton_ {nullptr};
   QFrame*                                  expandedPanel_ {nullptr};
   QWidget*                                 headerWidget_ {nullptr};
   QWidget*                                 toolsWidget_ {nullptr};
   QLabel*                                  titleLabel_ {nullptr};
   QPushButton*                             floatButton_ {nullptr};
   QPushButton*                             minimizeButton_ {nullptr};
   QButtonGroup*                            toolButtonGroup_ {nullptr};
   std::vector<QToolButton*>                toolButtons_ {};
   QComboBox*                               brushPresetCombo_ {nullptr};
   QWidget*                                 customBrushWidget_ {nullptr};
   QLabel*                                  customBrushValueLabel_ {nullptr};
   BrushScaleLabelsWidget*                  customBrushMarksWidget_ {nullptr};
   QSlider*                                 strokeWidthSlider_ {nullptr};
   QPushButton*                             colorButton_ {nullptr};
   QCheckBox*                               fillCheck_ {nullptr};
   QPushButton*                             clearButton_ {nullptr};
   QToolButton*                             visibilityButton_ {nullptr};
   std::shared_ptr<map::MapAnnotationLayer> layer_ {};
   QColor                                   strokeColor_ {255, 50, 50, 230};
   double                                   strokeWidthM_ {500.0};
   bool                                     drawingsVisible_ {true};

   std::function<std::vector<std::shared_ptr<map::MapAnnotationLayer>>()>
                                        getBroadcastLayers_ {};
   std::function<QWidget*()>            floatingDockHostResolver_ {};
   bool                                 suppressPersist_ {false};
   bool                                 pendingRestoreFloating_ {false};
   bool                                 legacyGlobalFloatingPos_ {false};
   std::vector<QMetaObject::Connection> connections_ {};
   std::string                          lastDistanceUnitsName_ {};
   boost::uuids::uuid                   distanceUnitsCallbackUuid_ {};
};

MapAnnotationDockWidget::MapAnnotationDockWidget(QWidget* parent) :
    QWidget(parent), p(std::make_unique<Impl>(this))
{
   setAttribute(Qt::WA_StyledBackground, true);
   setStyleSheet(QStringLiteral(
      "MapAnnotationDockWidget, #mapAnnotationExpandedPanel {"
      "background-color: rgba(32, 37, 43, 230);"
      "border: 1px solid rgba(255, 255, 255, 32);"
      "border-radius: 8px; }"
      "QToolButton {"
      "padding: 2px 6px;"
      "min-height: 22px;"
      "border: 1px solid rgba(255,255,255,52);"
      "border-radius: 5px;"
      "background-color: rgba(255,255,255,12);"
      "color: rgba(255,255,255,210); }"
      "QPushButton { min-height: 24px; }"
      "QToolButton:hover { background-color: rgba(255,255,255,20); }"
      "QToolButton:checked {"
      "background-color: rgba(66, 165, 245, 175);"
      "border: 2px solid rgba(255,255,255,235);"
      "color: white;"
      "font-weight: 700; }"));

   auto* mainLayout = new QVBoxLayout(this);
   mainLayout->setContentsMargins(0, 0, 0, 0);
   mainLayout->setSizeConstraint(QLayout::SetFixedSize);

   p->collapsedButton_ = new QPushButton(tr("Draw"), this);
   p->collapsedButton_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
   mainLayout->addWidget(p->collapsedButton_, 0, Qt::AlignHCenter);
   p->collapsedButton_->hide();
   p->collapsedButton_->installEventFilter(this);

   p->expandedPanel_ = new QFrame(this);
   p->expandedPanel_->setObjectName(
      QStringLiteral("mapAnnotationExpandedPanel"));
   p->expandedPanel_->installEventFilter(this);
   auto* expandedLayout = new QVBoxLayout(p->expandedPanel_);
   expandedLayout->setContentsMargins(8, 6, 8, 8);
   expandedLayout->setSpacing(5);
   expandedLayout->setSizeConstraint(QLayout::SetFixedSize);

   p->headerWidget_   = new QWidget(p->expandedPanel_);
   auto* headerLayout = new QHBoxLayout(p->headerWidget_);
   headerLayout->setContentsMargins(0, 0, 0, 0);
   headerLayout->setSpacing(4);
   p->titleLabel_  = new QLabel(tr("Draw"), p->headerWidget_);
   p->floatButton_ = new QPushButton(tr("Pop-Out"), p->headerWidget_);
   p->floatButton_->setFixedWidth(68);
   p->minimizeButton_ = new QPushButton(tr("-"), p->headerWidget_);
   p->minimizeButton_->setFixedWidth(24);
   headerLayout->addWidget(p->titleLabel_);
   headerLayout->addStretch(1);
   headerLayout->addWidget(p->floatButton_);
   headerLayout->addWidget(p->minimizeButton_);
   expandedLayout->addWidget(p->headerWidget_);
   p->headerWidget_->installEventFilter(this);

   p->toolsWidget_ = new QWidget(p->expandedPanel_);
   p->toolsWidget_->installEventFilter(this);
   auto* toolsLayout = new QGridLayout(p->toolsWidget_);
   toolsLayout->setContentsMargins(0, 0, 0, 0);
   toolsLayout->setHorizontalSpacing(4);
   toolsLayout->setVerticalSpacing(4);
   p->toolButtonGroup_ = new QButtonGroup(this);
   p->toolButtonGroup_->setExclusive(true);
   const std::array<std::pair<const char*, map::MapAnnotationTool>, 7> tools {{
      {"Off", map::MapAnnotationTool::None},
      {"Pen", map::MapAnnotationTool::Freehand},
      {"Line", map::MapAnnotationTool::Line},
      {"Circle", map::MapAnnotationTool::Circle},
      {"Rect", map::MapAnnotationTool::Rectangle},
      {"Meas", map::MapAnnotationTool::Measure},
      {"Erase", map::MapAnnotationTool::Erase},
   }};
   for (std::size_t i = 0; i < tools.size(); ++i)
   {
      auto* button = new QToolButton(p->toolsWidget_);
      button->setText(tr(tools[i].first));
      button->setCheckable(true);
      button->setAutoRaise(false);
      p->toolButtons_.push_back(button);
      p->toolButtonGroup_->addButton(button, static_cast<int>(tools[i].second));
      toolsLayout->addWidget(
         button, static_cast<int>(i / 4), static_cast<int>(i % 4));
   }
   p->visibilityButton_ = new QToolButton(p->toolsWidget_);
   p->visibilityButton_->setCheckable(true);
   p->visibilityButton_->setChecked(true);
   p->visibilityButton_->setText(tr("Hide"));
   p->visibilityButton_->setAutoRaise(false);
   const QSize hideSize = p->visibilityButton_->sizeHint();
   p->visibilityButton_->setText(tr("Show"));
   const QSize showSize = p->visibilityButton_->sizeHint();
   p->visibilityButton_->setFixedSize(hideSize.expandedTo(showSize));
   p->visibilityButton_->setText(tr("Hide"));
   toolsLayout->addWidget(p->visibilityButton_, 1, 3);
   int maxToolWidth  = 0;
   int maxToolHeight = 0;
   for (QToolButton* button : p->toolButtons_)
   {
      if (button == nullptr)
      {
         continue;
      }
      const QSize hint = button->sizeHint();
      maxToolWidth     = std::max(maxToolWidth, hint.width());
      maxToolHeight    = std::max(maxToolHeight, hint.height());
   }
   for (QToolButton* button : p->toolButtons_)
   {
      if (button != nullptr)
      {
         button->setFixedSize(maxToolWidth, maxToolHeight);
      }
   }
   expandedLayout->addWidget(p->toolsWidget_);

   auto* brushRow = new QHBoxLayout();
   brushRow->addWidget(new QLabel(tr("Brush size"), p->expandedPanel_));
   p->brushPresetCombo_ = new QComboBox(p->expandedPanel_);
   p->brushPresetCombo_->addItem(tr("Custom"), QVariant());
   for (int i = 0; i < 5; ++i)
   {
      p->brushPresetCombo_->addItem(QString(), QVariant());
   }
   brushRow->addWidget(p->brushPresetCombo_, 1);
   expandedLayout->addLayout(brushRow);

   p->customBrushWidget_ = new QWidget(p->expandedPanel_);
   p->customBrushWidget_->installEventFilter(this);
   auto* customBrushLayout = new QVBoxLayout(p->customBrushWidget_);
   customBrushLayout->setContentsMargins(0, 0, 0, 0);
   customBrushLayout->setSpacing(1);
   p->customBrushValueLabel_ = new QLabel(p->customBrushWidget_);
   p->strokeWidthSlider_ = new QSlider(Qt::Horizontal, p->customBrushWidget_);
   p->strokeWidthSlider_->setRange(0, kStrokeWidthSliderSteps);
   p->strokeWidthSlider_->setTickPosition(QSlider::TicksBelow);
   p->strokeWidthSlider_->setTickInterval(kStrokeWidthSliderSteps / 4);
   p->customBrushMarksWidget_ =
      new BrushScaleLabelsWidget(p->strokeWidthSlider_, p->customBrushWidget_);
   customBrushLayout->addWidget(p->customBrushValueLabel_);
   customBrushLayout->addWidget(p->strokeWidthSlider_);
   customBrushLayout->addWidget(p->customBrushMarksWidget_);
   expandedLayout->addWidget(p->customBrushWidget_);

   auto* actionRow = new QHBoxLayout();
   actionRow->setSpacing(4);
   p->colorButton_ = new QPushButton(p->expandedPanel_);
   p->colorButton_->setFixedWidth(36);
   p->colorButton_->setStyleSheet(
      QStringLiteral("background-color: %1")
         .arg(p->strokeColor_.name(QColor::HexArgb)));
   actionRow->addWidget(p->colorButton_);

   p->fillCheck_ = new QCheckBox(tr("Fill shape"), p->expandedPanel_);
   expandedLayout->addWidget(p->fillCheck_);

   p->clearButton_ = new QPushButton(tr("Clear"), p->expandedPanel_);
   actionRow->addWidget(p->clearButton_);
   expandedLayout->addLayout(actionRow);

   mainLayout->addWidget(p->expandedPanel_);

   connect(p->collapsedButton_,
           &QPushButton::clicked,
           this,
           &MapAnnotationDockWidget::OnToggleExpanded);
   connect(p->floatButton_,
           &QPushButton::clicked,
           this,
           [this]() { p->SetFloating(!p->floating_); });
   connect(p->minimizeButton_,
           &QPushButton::clicked,
           this,
           &MapAnnotationDockWidget::OnToggleExpanded);
   connect(p->toolButtonGroup_,
           QOverload<int>::of(&QButtonGroup::idClicked),
           this,
           &MapAnnotationDockWidget::OnToolSelected);
   connect(p->visibilityButton_,
           &QToolButton::toggled,
           this,
           [this](bool checked)
           {
              p->drawingsVisible_ = checked;
              p->visibilityButton_->setText(checked ? tr("Hide") : tr("Show"));
              for (const auto& L : p->LayersForToolStyle())
              {
                 if (L != nullptr)
                 {
                    L->SetVisible(checked);
                 }
              }
              p->SaveState();
           });
   connect(p->brushPresetCombo_,
           QOverload<int>::of(&QComboBox::currentIndexChanged),
           this,
           &MapAnnotationDockWidget::OnBrushPresetChanged);
   connect(p->strokeWidthSlider_,
           &QSlider::valueChanged,
           this,
           [this](int value)
           {
              p->strokeWidthM_ = StrokeWidthMetersFromSliderPosition(value);
              p->UpdateCustomBrushValueLabel();
              if (p->brushPresetCombo_->currentIndex() > 0)
              {
                 p->UpdateBrushPresetUiFromStrokeMeters(p->strokeWidthM_);
              }
              p->PushStyleFromUi();
              p->SaveState();
           });
   connect(p->fillCheck_,
           &QCheckBox::toggled,
           this,
           &MapAnnotationDockWidget::OnFillToggled);
   connect(p->colorButton_,
           &QPushButton::clicked,
           this,
           &MapAnnotationDockWidget::OnChooseColor);
   connect(p->clearButton_,
           &QPushButton::clicked,
           this,
           &MapAnnotationDockWidget::OnClearAll);

   p->distanceUnitsCallbackUuid_ =
      settings::UnitSettings::Instance()
         .distance_units()
         .RegisterValueChangedCallback(
            [impl = p.get()](const std::string& name)
            { impl->OnUserDistanceUnitsChanged(name); });

   p->SyncDistanceSpinBoxesToSettings();
   p->LoadState();
}

MapAnnotationDockWidget::~MapAnnotationDockWidget() = default;

void MapAnnotationDockWidget::DetachIfHostedBy(QWidget* mapWidget,
                                               bool     preserveFloating)
{
   if (thread() != QThread::currentThread())
   {
      QMetaObject::invokeMethod(
         this,
         [this, mapWidget, preserveFloating]()
         { DetachIfHostedBy(mapWidget, preserveFloating); },
         Qt::QueuedConnection);
      return;
   }

   if (mapWidget == nullptr || p->hostMapWidget_.data() != mapWidget)
   {
      return;
   }

   if (preserveFloating && p->floating_)
   {
      return;
   }

   for (const auto& layer : p->LayersForToolStyle())
   {
      if (layer != nullptr)
      {
         layer->SetTool(map::MapAnnotationTool::None);
      }
   }
   BindToLayer(nullptr, false);
   AttachToMap(nullptr);
}

void MapAnnotationDockWidget::AttachToMap(QWidget* mapWidget)
{
   if (thread() != QThread::currentThread())
   {
      QMetaObject::invokeMethod(
         this,
         [this, mapWidget]() { AttachToMap(mapWidget); },
         Qt::QueuedConnection);
      return;
   }

   if (p->hostMapWidget_ == mapWidget)
   {
      p->UpdatePlacement();
      return;
   }

   QWidget* const oldHostMapWidget = p->hostMapWidget_.data();
   if (oldHostMapWidget != nullptr &&
       oldHostMapWidget->thread() == QThread::currentThread())
   {
      oldHostMapWidget->removeEventFilter(this);
   }

   p->hostMapWidget_ = mapWidget;

   if (mapWidget == nullptr)
   {
      if (!p->floating_)
      {
         QWidget* const owner = !p->overlayParent_.isNull() ?
                                   p->overlayParent_.data() :
                                   parentWidget();
         if (owner != nullptr && owner != this && parentWidget() != owner)
         {
            setParent(owner, Qt::Widget);
         }
         hide();
      }
      return;
   }

   if (mapWidget->thread() != QThread::currentThread())
   {
      p->hostMapWidget_ = nullptr;
      if (!p->floating_)
      {
         hide();
      }
      return;
   }

   mapWidget->installEventFilter(this);
   p->UpdatePlacement();
}

void MapAnnotationDockWidget::SetOverlayVisible(bool visible)
{
   p->SetOverlayVisible(visible);
}

bool MapAnnotationDockWidget::OverlayVisible() const
{
   return p->overlayVisible_;
}

void MapAnnotationDockWidget::SetPanelExpanded(bool expanded)
{
   p->SetExpanded(expanded);
}

bool MapAnnotationDockWidget::PanelExpanded() const
{
   return p->expanded_;
}

void MapAnnotationDockWidget::BindToLayer(
   const std::shared_ptr<map::MapAnnotationLayer>& layer, bool syncUiFromLayer)
{
   p->DisconnectLayer();
   p->layer_ = layer;
   if (p->layer_ == nullptr)
   {
      return;
   }

   if (syncUiFromLayer)
   {
      p->PullStyleToUi();
   }

   ReapplyToolAndStyleFromUi();
}

void MapAnnotationDockWidget::SetBroadcastTargets(
   std::function<std::vector<std::shared_ptr<map::MapAnnotationLayer>>()>
      getLayers)
{
   p->getBroadcastLayers_ = std::move(getLayers);
}

void MapAnnotationDockWidget::SetFloatingDockHostResolver(
   std::function<QWidget*()> resolver)
{
   p->floatingDockHostResolver_ = std::move(resolver);
}

void MapAnnotationDockWidget::ApplyDeferredFloatingState()
{
   if (!p->pendingRestoreFloating_)
   {
      return;
   }
   p->pendingRestoreFloating_ = false;
   if (p->hostMapWidget_ == nullptr)
   {
      p->floatingPosition_.reset();
      p->UpdateFloatButtonText();
      p->SaveState();
      return;
   }
   p->SetFloating(true);
}

void MapAnnotationDockWidget::ReapplyToolAndStyleFromUi()
{
   const auto tool = p->CurrentTool();
   for (const auto& L : p->LayersForToolStyle())
   {
      if (L != nullptr)
      {
         L->SetVisible(p->drawingsVisible_);
         L->SetTool(tool);
      }
   }
   p->PushStyleFromUi();
   p->UpdateFillVisibility();
}

void MapAnnotationDockWidget::OnToolSelected(int toolValue)
{
   const auto tool = static_cast<map::MapAnnotationTool>(toolValue);
   for (const auto& L : p->LayersForToolStyle())
   {
      if (L != nullptr)
      {
         L->SetTool(tool);
      }
   }
   p->UpdateFillVisibility();
   p->SaveState();
}

void MapAnnotationDockWidget::OnBrushPresetChanged(int index)
{
   p->UpdateBrushControlVisibility();
   if (index <= 0 || p->brushPresetCombo_ == nullptr)
   {
      p->SaveState();
      return;
   }
   bool         ok     = false;
   const double meters = p->brushPresetCombo_->itemData(index).toDouble(&ok);
   if (!ok)
   {
      return;
   }
   p->strokeWidthM_ = meters;
   p->UpdateCustomBrushSlider();
   p->UpdateCustomBrushValueLabel();
   p->PushStyleFromUi();
   p->SaveState();
}

void MapAnnotationDockWidget::OnFillToggled(bool /*on*/)
{
   p->PushStyleFromUi();
   p->SaveState();
}

void MapAnnotationDockWidget::OnChooseColor()
{
   const QColor c =
      QColorDialog::getColor(p->strokeColor_, this, tr("Stroke color"));
   if (c.isValid())
   {
      p->strokeColor_ = c;
      p->colorButton_->setStyleSheet(
         QStringLiteral("background-color: %1")
            .arg(p->strokeColor_.name(QColor::HexArgb)));
      p->PushStyleFromUi();
      p->SaveState();
   }
}

void MapAnnotationDockWidget::OnClearAll()
{
   for (const auto& L : p->LayersForToolStyle())
   {
      if (L != nullptr)
      {
         L->ClearAll();
      }
   }
}

void MapAnnotationDockWidget::OnToggleExpanded()
{
   p->SetExpanded(!p->expanded_);
}

bool MapAnnotationDockWidget::eventFilter(QObject* watched, QEvent* event)
{
   if (watched == p->hostMapWidget_ &&
       (event->type() == QEvent::Resize || event->type() == QEvent::Move ||
        event->type() == QEvent::Show))
   {
      p->UpdatePlacement();
   }
   else if (watched == p->collapsedButton_ || watched == p->headerWidget_)
   {
      if (event->type() == QEvent::MouseButtonPress)
      {
         auto* mouseEvent = dynamic_cast<QMouseEvent*>(event);
         if (mouseEvent == nullptr)
         {
            return QWidget::eventFilter(watched, event);
         }
         if (mouseEvent->button() == Qt::LeftButton)
         {
            p->dragStartGlobal_ = mouseEvent->globalPosition().toPoint();
            if (p->floating_)
            {
               p->dragStartPosition_ = pos();
            }
            else
            {
               p->dragStartOverlayGlobal_ = mapToGlobal(QPoint {0, 0});
            }
            p->dragging_ = false;
         }
      }
      else if (event->type() == QEvent::MouseMove)
      {
         auto* mouseEvent = dynamic_cast<QMouseEvent*>(event);
         if (mouseEvent == nullptr)
         {
            return QWidget::eventFilter(watched, event);
         }
         if ((mouseEvent->buttons() & Qt::LeftButton) == 0)
         {
            return QWidget::eventFilter(watched, event);
         }

         const QPoint delta =
            mouseEvent->globalPosition().toPoint() - p->dragStartGlobal_;
         if (!p->dragging_ &&
             delta.manhattanLength() < QApplication::startDragDistance())
         {
            return QWidget::eventFilter(watched, event);
         }

         p->dragging_ = true;

         if (p->floating_)
         {
            p->floatingPosition_        = p->dragStartPosition_ + delta;
            const auto floatingPosition = p->floatingPosition_;
            if (floatingPosition.has_value())
            {
               move(*floatingPosition);
            }
         }
         else if (p->hostMapWidget_ != nullptr)
         {
            const QPoint newTopLeftGlobal = p->dragStartOverlayGlobal_ + delta;
            p->attachedPosition_          = ClampOverlayPosition(
               p->hostMapWidget_,
               this,
               p->hostMapWidget_->mapFromGlobal(newTopLeftGlobal));
            const auto attachedPosition = p->attachedPosition_;
            if (attachedPosition.has_value())
            {
               QWidget* const op = parentWidget();
               if (op != nullptr)
               {
                  move(p->hostMapWidget_->mapTo(op, *attachedPosition));
               }
               else
               {
                  move(p->hostMapWidget_->mapToGlobal(*attachedPosition));
               }
            }
         }
         return true;
      }
      else if (event->type() == QEvent::MouseButtonRelease)
      {
         const bool wasDragging = p->dragging_;
         p->dragging_           = false;
         if (wasDragging)
         {
            p->SaveState();
            return true;
         }
      }

      if (watched == p->headerWidget_ &&
          (event->type() == QEvent::MouseButtonPress ||
           event->type() == QEvent::MouseButtonRelease ||
           event->type() == QEvent::MouseButtonDblClick))
      {
         event->accept();
         return true;
      }
   }
   else if (watched == p->expandedPanel_ || watched == p->toolsWidget_ ||
            watched == p->customBrushWidget_)
   {
      switch (event->type())
      {
      case QEvent::MouseButtonPress:
      case QEvent::MouseButtonRelease:
      case QEvent::MouseButtonDblClick:
      case QEvent::MouseMove:
      case QEvent::Wheel:
         event->accept();
         return true;

      default:
         break;
      }
   }
   return QWidget::eventFilter(watched, event);
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,cppcoreguidelines-owning-memory,cppcoreguidelines-pro-bounds-constant-array-index,bugprone-narrowing-conversions,cppcoreguidelines-narrowing-conversions)
} // namespace scwx::qt::ui
