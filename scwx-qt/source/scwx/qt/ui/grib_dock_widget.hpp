#pragma once

#include <scwx/qt/map/grib_frame_info.hpp>

#include <memory>

#include <QDockWidget>
#include <QString>

namespace scwx::qt::ui
{

// One combined dock (not one per category, see supercell-wx-grib-
// extension memory for that earlier design and why it changed) hosting a
// section per map::GribCategory (MRMS/RTMA/RRFS today) -- each section's
// CheckableComboBox drives that category's GribManager::SetProductActive,
// letting several products be shown at once within one category rather
// than a plain single-select dropdown. Built entirely in code (no .ui
// file) since the three sections are otherwise identical and would
// otherwise triple the same layout in Designer XML.
class GribDockWidget : public QDockWidget
{
   Q_OBJECT

public:
   explicit GribDockWidget(QWidget* parent = nullptr);
   ~GribDockWidget();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::ui
