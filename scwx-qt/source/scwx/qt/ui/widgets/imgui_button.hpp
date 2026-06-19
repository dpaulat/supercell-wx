#pragma once

#include <scwx/qt/settings/button_settings.hpp>

#include <memory>

#include <boost/gil/typedefs.hpp>
#include <QPushButton>

namespace scwx::qt::ui
{

class ImGuiButton : public QPushButton
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(ImGuiButton)

public:
   explicit ImGuiButton(QWidget* parent = nullptr);
   ~ImGuiButton() override;

   void set_button_settings(settings::ButtonSettings& buttonSettings);

   void SetStyle(const boost::gil::rgba8_pixel_t& buttonColor,
                 const boost::gil::rgba8_pixel_t& hoverColor,
                 const boost::gil::rgba8_pixel_t& activeColor);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::ui
