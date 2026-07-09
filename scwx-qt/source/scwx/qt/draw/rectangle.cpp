#include <scwx/qt/draw/rectangle.hpp>
#include <scwx/util/logger.hpp>

#include <scwx/qt/render/projection.hpp>
#include <scwx/qt/render/rhi_colored_geometry.hpp>
#include <scwx/qt/render/rhi_vulkan_overlay.hpp>

#include <optional>

namespace scwx::qt::draw
{

static const std::string logPrefix_ = "scwx::qt::draw::rectangle";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr size_t NUM_RECTANGLES         = 5;
static constexpr size_t NUM_TRIANGLES          = NUM_RECTANGLES * 2;
static constexpr size_t VERTICES_PER_TRIANGLE  = 3;
static constexpr size_t VERTICES_PER_RECTANGLE = VERTICES_PER_TRIANGLE * 2;
static constexpr size_t POINTS_PER_VERTEX      = 7;
static constexpr size_t BUFFER_LENGTH =
   NUM_TRIANGLES * VERTICES_PER_TRIANGLE * POINTS_PER_VERTEX;

class Rectangle::Impl
{
public:
   explicit Impl(std::shared_ptr<render::RenderContext> context) :
       context_ {std::move(context)},
       dirty_ {false},
       visible_ {true},
       x_ {0.0f},
       y_ {0.0f},
       z_ {0.0f},
       width_ {0.0f},
       height_ {0.0f},
       borderWidth_ {0.0f},
       borderColor_ {0, 0, 0, 0},
       fillColor_ {std::nullopt}
   {
   }
   ~Impl() = default;

   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   std::shared_ptr<render::RenderContext> context_;

   bool dirty_;

   bool  visible_;
   float x_;
   float y_;
   float z_;
   float width_;
   float height_;

   float                     borderWidth_;
   boost::gil::rgba8_pixel_t borderColor_;

   std::optional<boost::gil::rgba8_pixel_t> fillColor_;

   std::vector<float> vertexBuffer_;

   void Update();
   void BuildVertexBuffer();
};

Rectangle::Rectangle(std::shared_ptr<render::RenderContext> context) :
    DrawItem(), p(std::make_unique<Impl>(context))
{
}
Rectangle::~Rectangle() = default;

Rectangle::Rectangle(Rectangle&&) noexcept            = default;
Rectangle& Rectangle::operator=(Rectangle&&) noexcept = default;

void Rectangle::Initialize()
{
   p->dirty_ = true;
}

void Rectangle::Render(
   const QMapLibre::CustomLayerRenderParameters& /* params */)
{
}

void Rectangle::RenderVulkan(
   QRhiCommandBuffer*                            commandBuffer,
   render::RhiVulkanOverlayResources&            resources,
   const QMapLibre::CustomLayerRenderParameters& params,
   bool /* textureAtlasChanged */)
{
   if (!p->visible_)
   {
      return;
   }

   p->Update();

   const glm::mat4 projection = scwx::qt::render::OrthoMapProjection(params);

   if (p->fillColor_.has_value())
   {
      const std::vector<float> fillVertices(
         p->vertexBuffer_.begin() +
            static_cast<std::ptrdiff_t>(24 * POINTS_PER_VERTEX),
         p->vertexBuffer_.end());
      resources.coloredGeometry.Render(
         commandBuffer, projection, fillVertices, 6, resources.resourceBatch,
         resources.phase);
   }

   if (p->borderWidth_ > 0.0f)
   {
      resources.coloredGeometry.Render(
         commandBuffer, projection, p->vertexBuffer_, 24,
         resources.resourceBatch, resources.phase);
   }
}

void Rectangle::Deinitialize()
{
   p->vertexBuffer_.clear();
}

void Rectangle::SetBorder(float width, boost::gil::rgba8_pixel_t color)
{
   if (p->borderWidth_ != width || p->borderColor_ != color)
   {
      p->borderWidth_ = width;
      p->borderColor_ = color;
      p->dirty_       = true;
   }
}

void Rectangle::SetFill(boost::gil::rgba8_pixel_t color)
{
   if (p->fillColor_ != color)
   {
      p->fillColor_ = color;
      p->dirty_     = true;
   }
}

void Rectangle::SetPosition(float x, float y, float z)
{
   if (p->x_ != x || p->y_ != y || p->z_ != z)
   {
      p->x_     = x;
      p->y_     = y;
      p->z_     = z;
      p->dirty_ = true;
   }
}

void Rectangle::SetSize(float width, float height)
{
   if (p->width_ != width || p->height_ != height)
   {
      p->width_  = width;
      p->height_ = height;
      p->dirty_  = true;
   }
}

void Rectangle::SetVisible(bool visible)
{
   p->visible_ = visible;
}

void Rectangle::Impl::Update()
{
   if (dirty_)
   {
      BuildVertexBuffer();
      dirty_ = false;
   }
}

void Rectangle::Impl::BuildVertexBuffer()
{
   const float lox = x_;
   const float rox = x_ + width_;
   const float boy = y_;
   const float toy = y_ + height_;

   const float lix = lox + borderWidth_;
   const float rix = rox - borderWidth_;

   const float biy = boy + borderWidth_;
   const float tiy = toy - borderWidth_;

   const float bc0 = borderColor_[0] / 255.0f;
   const float bc1 = borderColor_[1] / 255.0f;
   const float bc2 = borderColor_[2] / 255.0f;
   const float bc3 = borderColor_[3] / 255.0f;

   float fc0 = 0.0f;
   float fc1 = 0.0f;
   float fc2 = 0.0f;
   float fc3 = 0.0f;

   if (fillColor_.has_value())
   {
      boost::gil::rgba8_pixel_t& fc = fillColor_.value();

      fc0 = fc[0] / 255.0f;
      fc1 = fc[1] / 255.0f;
      fc2 = fc[2] / 255.0f;
      fc3 = fc[3] / 255.0f;
   }

   const float buffer[NUM_RECTANGLES][VERTICES_PER_RECTANGLE]
                     [POINTS_PER_VERTEX] = //
      {                                    //

       // Left Border
       {
          {lox, boy, z_, bc0, bc1, bc2, bc3}, // BL
          {lox, toy, z_, bc0, bc1, bc2, bc3}, // TL
          {lix, boy, z_, bc0, bc1, bc2, bc3}, // BR
          {lix, boy, z_, bc0, bc1, bc2, bc3}, // BR
          {lix, toy, z_, bc0, bc1, bc2, bc3}, // TR
          {lox, toy, z_, bc0, bc1, bc2, bc3}  // TL
       },
       // Right Border
       {
          {rox, boy, z_, bc0, bc1, bc2, bc3}, // BR
          {rox, toy, z_, bc0, bc1, bc2, bc3}, // TR
          {rix, boy, z_, bc0, bc1, bc2, bc3}, // BL
          {rix, boy, z_, bc0, bc1, bc2, bc3}, // BL
          {rix, toy, z_, bc0, bc1, bc2, bc3}, // TL
          {rox, toy, z_, bc0, bc1, bc2, bc3}  // TR
       },
       // Top Border
       {
          {lox, toy, z_, bc0, bc1, bc2, bc3}, // TL
          {rox, toy, z_, bc0, bc1, bc2, bc3}, // TR
          {rox, tiy, z_, bc0, bc1, bc2, bc3}, // BR
          {rox, tiy, z_, bc0, bc1, bc2, bc3}, // BR
          {lox, tiy, z_, bc0, bc1, bc2, bc3}, // BL
          {lox, toy, z_, bc0, bc1, bc2, bc3}  // TL
       },
       // Bottom Border
       {
          {lox, boy, z_, bc0, bc1, bc2, bc3}, // BL
          {rox, boy, z_, bc0, bc1, bc2, bc3}, // BR
          {rox, biy, z_, bc0, bc1, bc2, bc3}, // TR
          {rox, biy, z_, bc0, bc1, bc2, bc3}, // TR
          {lox, biy, z_, bc0, bc1, bc2, bc3}, // TL
          {lox, boy, z_, bc0, bc1, bc2, bc3}  // BL
       },
       // Fill
       {
          {lox, toy, z_, fc0, fc1, fc2, fc3}, // TL
          {rox, toy, z_, fc0, fc1, fc2, fc3}, // TR
          {rox, boy, z_, fc0, fc1, fc2, fc3}, // BR
          {rox, boy, z_, fc0, fc1, fc2, fc3}, // BR
          {lox, boy, z_, fc0, fc1, fc2, fc3}, // BL
          {lox, toy, z_, fc0, fc1, fc2, fc3}  // TL
       }};

   vertexBuffer_.assign(&buffer[0][0][0], &buffer[0][0][0] + BUFFER_LENGTH);
}

} // namespace scwx::qt::draw
