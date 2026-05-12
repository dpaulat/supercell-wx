#pragma once

#include <scwx/sounding/sounding_data.hpp>

#include <memory>

#include <QWidget>

namespace scwx::qt::ui
{

class SoundingParametersWidget : public QWidget
{
   Q_OBJECT

public:
   explicit SoundingParametersWidget(QWidget* parent = nullptr);
   ~SoundingParametersWidget();

   void SetSounding(const std::shared_ptr<sounding::SoundingData>& sounding);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::ui
