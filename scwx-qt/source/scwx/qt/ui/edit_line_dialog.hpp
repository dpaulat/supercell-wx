#pragma once

#include <QDialog>

#include <boost/gil/typedefs.hpp>

namespace Ui
{
class EditLineDialog;
}

namespace scwx
{
namespace qt
{
namespace ui
{

class EditLineDialog : public QDialog
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(EditLineDialog)

public:
   explicit EditLineDialog(QWidget* parent = nullptr);
   ~EditLineDialog();

   boost::gil::rgba8_pixel_t border_color() const;
   boost::gil::rgba8_pixel_t highlight_color() const;
   boost::gil::rgba8_pixel_t line_color() const;

   std::size_t border_width() const;
   std::size_t highlight_width() const;
   std::size_t line_width() const;

   void set_border_color(boost::gil::rgba8_pixel_t color);
   void set_highlight_color(boost::gil::rgba8_pixel_t color);
   void set_line_color(boost::gil::rgba8_pixel_t color);

   void set_border_width(std::size_t width);
   void set_highlight_width(std::size_t width);
   void set_line_width(std::size_t width);

   void Initialize(boost::gil::rgba8_pixel_t borderColor,
                   boost::gil::rgba8_pixel_t highlightColor,
                   boost::gil::rgba8_pixel_t lineColor,
                   std::size_t               borderWidth,
                   std::size_t               highlightWidth,
                   std::size_t               lineWidth);

private:
   class Impl;
   std::unique_ptr<Impl> p;
   Ui::EditLineDialog*   ui;
};

} // namespace ui
} // namespace qt
} // namespace scwx
