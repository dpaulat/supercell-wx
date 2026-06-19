#pragma once

#include <QDialog>

#include <boost/gil/typedefs.hpp>

namespace Ui
{
class EditButtonDialog;
}

namespace scwx::qt::ui
{

class EditButtonDialog : public QDialog
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(EditButtonDialog)

public:
   explicit EditButtonDialog(QWidget* parent = nullptr);
   ~EditButtonDialog() override;

   [[nodiscard]] boost::gil::rgba8_pixel_t active_color() const;
   [[nodiscard]] boost::gil::rgba8_pixel_t button_color() const;
   [[nodiscard]] boost::gil::rgba8_pixel_t hover_color() const;

   void set_active_color(const boost::gil::rgba8_pixel_t& color);
   void set_button_color(const boost::gil::rgba8_pixel_t& color);
   void set_hover_color(const boost::gil::rgba8_pixel_t& color);

   void Initialize(const boost::gil::rgba8_pixel_t& activeColor,
                   const boost::gil::rgba8_pixel_t& buttonColor,
                   const boost::gil::rgba8_pixel_t& hoverColor);

private:
   class Impl;
   std::unique_ptr<Impl> p;
   Ui::EditButtonDialog* ui;
};

} // namespace scwx::qt::ui
