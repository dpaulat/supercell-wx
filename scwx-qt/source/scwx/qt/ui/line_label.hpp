#pragma once

#include <scwx/qt/settings/line_settings.hpp>

#include <QFrame>

#include <boost/gil/typedefs.hpp>

namespace scwx
{
namespace qt
{
namespace ui
{

class LineLabel : public QFrame
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(LineLabel)

public:
   explicit LineLabel(QWidget* parent = nullptr);
   ~LineLabel();

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

   void set_line_settings(settings::LineSettings& lineSettings);

protected:
   QSize minimumSizeHint() const override;
   QSize sizeHint() const override;
   void  paintEvent(QPaintEvent* e) override;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace ui
} // namespace qt
} // namespace scwx
