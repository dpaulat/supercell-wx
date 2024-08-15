#include <scwx/qt/ui/line_label.hpp>
#include <scwx/util/logger.hpp>

#include <QEvent>
#include <QPainter>

namespace scwx
{
namespace qt
{
namespace ui
{

static const std::string logPrefix_ = "scwx::qt::ui::line_label";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class LineLabel::Impl
{
public:
   explicit Impl() {};
   ~Impl() = default;

   QImage GenerateImage() const;

   std::size_t borderWidth_ {1};
   std::size_t highlightWidth_ {1};
   std::size_t lineWidth_ {3};

   boost::gil::rgba8_pixel_t borderColor_ {0, 0, 0, 255};
   boost::gil::rgba8_pixel_t highlightColor_ {255, 255, 0, 255};
   boost::gil::rgba8_pixel_t lineColor_ {0, 0, 255, 255};

   QPixmap pixmap_ {};
   bool    pixmapDirty_ {true};
};

LineLabel::LineLabel(QWidget* parent) :
    QFrame(parent), p {std::make_unique<Impl>()}
{
}

LineLabel::~LineLabel() {}

void LineLabel::set_border_width(std::size_t width)
{
   p->borderWidth_ = width;
   p->pixmapDirty_ = true;
   update();
}

void LineLabel::set_highlight_width(std::size_t width)
{
   p->highlightWidth_ = width;
   p->pixmapDirty_    = true;
   update();
}

void LineLabel::set_line_width(std::size_t width)
{
   p->lineWidth_   = width;
   p->pixmapDirty_ = true;
   update();
}

void LineLabel::set_border_color(boost::gil::rgba8_pixel_t color)
{
   p->borderColor_ = color;
   p->pixmapDirty_ = true;
   update();
}

void LineLabel::set_highlight_color(boost::gil::rgba8_pixel_t color)
{
   p->highlightColor_ = color;
   p->pixmapDirty_    = true;
   update();
}

void LineLabel::set_line_color(boost::gil::rgba8_pixel_t color)
{
   p->lineColor_   = color;
   p->pixmapDirty_ = true;
   update();
}

QSize LineLabel::minimumSizeHint() const
{
   return sizeHint();
}

QSize LineLabel::sizeHint() const
{
   QMargins margins = contentsMargins();

   const std::size_t width = 1;
   const std::size_t height =
      (p->borderWidth_ + p->highlightWidth_) * 2 + p->lineWidth_;

   return QSize(static_cast<int>(width) + margins.left() + margins.right(),
                static_cast<int>(height) + margins.top() + margins.bottom());
}

void LineLabel::paintEvent(QPaintEvent* e)
{
   logger_->trace("paintEvent");

   QFrame::paintEvent(e);

   if (p->pixmapDirty_)
   {
      QImage image    = p->GenerateImage();
      p->pixmap_      = QPixmap::fromImage(image);
      p->pixmapDirty_ = false;
   }

   // Don't stretch the line pixmap vertically
   QRect rect = contentsRect();
   if (rect.height() > p->pixmap_.height())
   {
      int dy  = rect.height() - p->pixmap_.height();
      int dy1 = dy / 2;
      int dy2 = dy - dy1;
      rect.adjust(0, dy1, 0, -dy2);
   }

   QPainter painter(this);
   painter.drawPixmap(rect, p->pixmap_);
}

QImage LineLabel::Impl::GenerateImage() const
{
   const QRgb borderRgba    = qRgba(static_cast<int>(borderColor_[0]),
                                 static_cast<int>(borderColor_[1]),
                                 static_cast<int>(borderColor_[2]),
                                 static_cast<int>(borderColor_[3]));
   const QRgb highlightRgba = qRgba(static_cast<int>(highlightColor_[0]),
                                    static_cast<int>(highlightColor_[1]),
                                    static_cast<int>(highlightColor_[2]),
                                    static_cast<int>(highlightColor_[3]));
   const QRgb lineRgba      = qRgba(static_cast<int>(lineColor_[0]),
                               static_cast<int>(lineColor_[1]),
                               static_cast<int>(lineColor_[2]),
                               static_cast<int>(lineColor_[3]));

   const std::size_t width  = 1;
   const std::size_t height = (borderWidth_ + highlightWidth_) * 2 + lineWidth_;

   QImage image(static_cast<int>(width),
                static_cast<int>(height),
                QImage::Format::Format_ARGB32);

   std::size_t y = 0;
   for (std::size_t i = 0; i < borderWidth_; ++i, ++y)
   {
      image.setPixel(0, static_cast<int>(y), borderRgba);
      image.setPixel(0, static_cast<int>(height - 1 - y), borderRgba);
   }

   for (std::size_t i = 0; i < highlightWidth_; ++i, ++y)
   {
      image.setPixel(0, static_cast<int>(y), highlightRgba);
      image.setPixel(0, static_cast<int>(height - 1 - y), highlightRgba);
   }

   for (std::size_t i = 0; i < lineWidth_; ++i, ++y)
   {
      image.setPixel(0, static_cast<int>(y), lineRgba);
      image.setPixel(0, static_cast<int>(height - 1 - y), lineRgba);
   }

   return image;
}

} // namespace ui
} // namespace qt
} // namespace scwx
