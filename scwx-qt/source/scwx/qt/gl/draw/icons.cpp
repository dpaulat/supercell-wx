#include <scwx/qt/gl/draw/icons.hpp>
#include <scwx/qt/util/maplibre.hpp>
#include <scwx/qt/util/texture_atlas.hpp>
#include <scwx/qt/util/tooltip.hpp>
#include <scwx/util/logger.hpp>

#include <execution>

#include <boost/unordered/unordered_flat_map.hpp>

namespace scwx
{
namespace qt
{
namespace gl
{
namespace draw
{

static const std::string logPrefix_ = "scwx::qt::gl::draw::icons";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr std::size_t kNumRectangles        = 1;
static constexpr std::size_t kNumTriangles         = kNumRectangles * 2;
static constexpr std::size_t kVerticesPerTriangle  = 3;
static constexpr std::size_t kVerticesPerRectangle = kVerticesPerTriangle * 2;
static constexpr std::size_t kPointsPerVertex      = 9;
static constexpr std::size_t kPointsPerTexCoord    = 3;
static constexpr std::size_t kIconBufferLength =
   kNumTriangles * kVerticesPerTriangle * kPointsPerVertex;
static constexpr std::size_t kTextureBufferLength =
   kNumTriangles * kVerticesPerTriangle * kPointsPerTexCoord;

struct IconDrawItem : types::EventHandler
{
   boost::gil::rgba32f_pixel_t modulate_ {1.0f, 1.0f, 1.0f, 1.0f};
   double                      x_ {};
   double                      y_ {};
   units::degrees<double>      angle_ {};
   std::string                 iconSheet_ {};
   std::size_t                 iconIndex_ {};
   std::string                 hoverText_ {};
};

class Icons::Impl
{
public:
   struct IconHoverEntry
   {
      std::shared_ptr<IconDrawItem> di_;

      glm::vec2 otl_;
      glm::vec2 otr_;
      glm::vec2 obl_;
      glm::vec2 obr_;
   };

   explicit Impl(const std::shared_ptr<GlContext>& context) :
       context_ {context},
       shaderProgram_ {nullptr},
       uMVPMatrixLocation_(GL_INVALID_INDEX),
       vao_ {GL_INVALID_INDEX},
       vbo_ {GL_INVALID_INDEX},
       numVertices_ {0}
   {
   }

   ~Impl() {}

   void UpdateBuffers();
   void UpdateTextureBuffer();
   void Update(bool textureAtlasChanged);

   std::shared_ptr<GlContext> context_;

   bool visible_ {true};
   bool dirty_ {false};
   bool lastTextureAtlasChanged_ {false};

   std::mutex iconMutex_;

   boost::unordered_flat_map<std::string, std::shared_ptr<types::IconInfo>>
      currentIconSheets_ {};
   boost::unordered_flat_map<std::string, std::shared_ptr<types::IconInfo>>
      newIconSheets_ {};

   std::vector<std::shared_ptr<IconDrawItem>> currentIconList_ {};
   std::vector<std::shared_ptr<IconDrawItem>> newIconList_ {};
   std::vector<std::shared_ptr<IconDrawItem>> newValidIconList_ {};

   std::vector<float> currentIconBuffer_ {};
   std::vector<float> newIconBuffer_ {};

   std::vector<float> textureBuffer_ {};

   std::vector<IconHoverEntry> currentHoverIcons_ {};
   std::vector<IconHoverEntry> newHoverIcons_ {};

   std::shared_ptr<ShaderProgram> shaderProgram_;
   GLint                          uMVPMatrixLocation_;

   GLuint                vao_;
   std::array<GLuint, 2> vbo_;

   GLsizei numVertices_;
};

Icons::Icons(const std::shared_ptr<GlContext>& context) :
    DrawItem(context->gl()), p(std::make_unique<Impl>(context))
{
}
Icons::~Icons() = default;

Icons::Icons(Icons&&) noexcept            = default;
Icons& Icons::operator=(Icons&&) noexcept = default;

void Icons::Initialize()
{
   gl::OpenGLFunctions& gl = p->context_->gl();

   p->shaderProgram_ = p->context_->GetShaderProgram(
      {{GL_VERTEX_SHADER, ":/gl/texture2d_array.vert"},
       {GL_FRAGMENT_SHADER, ":/gl/texture2d_array.frag"}});

   p->uMVPMatrixLocation_ = p->shaderProgram_->GetUniformLocation("uMVPMatrix");

   gl.glGenVertexArrays(1, &p->vao_);
   gl.glGenBuffers(static_cast<GLsizei>(p->vbo_.size()), p->vbo_.data());

   gl.glBindVertexArray(p->vao_);
   gl.glBindBuffer(GL_ARRAY_BUFFER, p->vbo_[0]);
   gl.glBufferData(GL_ARRAY_BUFFER, 0u, nullptr, GL_DYNAMIC_DRAW);

   // aVertex
   gl.glVertexAttribPointer(0,
                            2,
                            GL_FLOAT,
                            GL_FALSE,
                            kPointsPerVertex * sizeof(float),
                            reinterpret_cast<void*>(0));
   gl.glEnableVertexAttribArray(0);

   // aXYOffset
   gl.glVertexAttribPointer(1,
                            2,
                            GL_FLOAT,
                            GL_FALSE,
                            kPointsPerVertex * sizeof(float),
                            reinterpret_cast<void*>(2 * sizeof(float)));
   gl.glEnableVertexAttribArray(1);

   // aModulate
   gl.glVertexAttribPointer(3,
                            4,
                            GL_FLOAT,
                            GL_FALSE,
                            kPointsPerVertex * sizeof(float),
                            reinterpret_cast<void*>(4 * sizeof(float)));
   gl.glEnableVertexAttribArray(3);

   // aAngle
   gl.glVertexAttribPointer(4,
                            1,
                            GL_FLOAT,
                            GL_FALSE,
                            kPointsPerVertex * sizeof(float),
                            reinterpret_cast<void*>(8 * sizeof(float)));
   gl.glEnableVertexAttribArray(4);

   gl.glBindBuffer(GL_ARRAY_BUFFER, p->vbo_[1]);
   gl.glBufferData(GL_ARRAY_BUFFER, 0u, nullptr, GL_DYNAMIC_DRAW);

   // aTexCoord
   gl.glVertexAttribPointer(2,
                            3,
                            GL_FLOAT,
                            GL_FALSE,
                            kPointsPerTexCoord * sizeof(float),
                            static_cast<void*>(0));
   gl.glEnableVertexAttribArray(2);

   p->dirty_ = true;
}

void Icons::Render(const QMapLibre::CustomLayerRenderParameters& params,
                   bool textureAtlasChanged)
{
   if (!p->visible_)
   {
      if (textureAtlasChanged)
      {
         p->lastTextureAtlasChanged_ = true;
      }

      return;
   }

   std::unique_lock lock {p->iconMutex_};

   if (!p->currentIconList_.empty())
   {
      gl::OpenGLFunctions& gl = p->context_->gl();

      gl.glBindVertexArray(p->vao_);

      p->Update(textureAtlasChanged);
      p->shaderProgram_->Use();
      UseDefaultProjection(params, p->uMVPMatrixLocation_);

      // Interpolate texture coordinates
      gl.glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      gl.glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

      // Draw icons
      gl.glDrawArrays(GL_TRIANGLES, 0, p->numVertices_);
   }
}

void Icons::Deinitialize()
{
   gl::OpenGLFunctions& gl = p->context_->gl();

   gl.glDeleteVertexArrays(1, &p->vao_);
   gl.glDeleteBuffers(static_cast<GLsizei>(p->vbo_.size()), p->vbo_.data());

   std::unique_lock lock {p->iconMutex_};

   p->currentIconList_.clear();
   p->currentIconSheets_.clear();
   p->currentHoverIcons_.clear();
   p->currentIconBuffer_.clear();
   p->textureBuffer_.clear();
}

void Icons::SetVisible(bool visible)
{
   p->visible_ = visible;
}

void Icons::StartIconSheets()
{
   // Clear the new buffer
   p->newIconSheets_.clear();
}

std::shared_ptr<types::IconInfo> Icons::AddIconSheet(const std::string& name,
                                                     std::size_t  iconWidth,
                                                     std::size_t  iconHeight,
                                                     std::int32_t hotX,
                                                     std::int32_t hotY)
{
   // Populate icon sheet map
   return p->newIconSheets_
      .emplace(std::piecewise_construct,
               std::tuple {name},
               std::forward_as_tuple(std::make_shared<types::IconInfo>(
                  name, iconWidth, iconHeight, hotX, hotY)))
      .first->second;
}

void Icons::FinishIconSheets()
{
   // Update icon sheets
   for (auto& iconSheet : p->newIconSheets_)
   {
      iconSheet.second->UpdateTextureInfo();
   }

   std::unique_lock lock {p->iconMutex_};

   // Swap buffers
   p->currentIconSheets_.swap(p->newIconSheets_);

   // Clear the new buffers
   p->newIconSheets_.clear();

   // Mark the draw item dirty
   p->dirty_ = true;
}

void Icons::StartIcons()
{
   // Clear the new buffer
   p->newIconList_.clear();
   p->newValidIconList_.clear();
   p->newIconBuffer_.clear();
   p->newHoverIcons_.clear();
}

std::shared_ptr<IconDrawItem> Icons::AddIcon()
{
   return p->newIconList_.emplace_back(std::make_shared<IconDrawItem>());
}

void Icons::SetIconTexture(const std::shared_ptr<IconDrawItem>& di,
                           const std::string&                   iconSheet,
                           std::size_t                          iconIndex)
{
   di->iconSheet_ = iconSheet;
   di->iconIndex_ = iconIndex;
}

void Icons::SetIconLocation(const std::shared_ptr<IconDrawItem>& di,
                            double                               x,
                            double                               y)
{
   di->x_ = x;
   di->y_ = y;
}

void Icons::SetIconAngle(const std::shared_ptr<IconDrawItem>& di,
                         units::angle::degrees<double>        angle)
{
   di->angle_ = angle;
}

void Icons::SetIconModulate(const std::shared_ptr<IconDrawItem>& di,
                            boost::gil::rgba8_pixel_t            modulate)
{
   di->modulate_ = {modulate[0] / 255.0f,
                    modulate[1] / 255.0f,
                    modulate[2] / 255.0f,
                    modulate[3] / 255.0f};
}

void Icons::SetIconModulate(const std::shared_ptr<IconDrawItem>& di,
                            boost::gil::rgba32f_pixel_t          modulate)
{
   di->modulate_ = modulate;
}

void Icons::SetIconHoverText(const std::shared_ptr<IconDrawItem>& di,
                             const std::string&                   text)
{
   di->hoverText_ = text;
}

void Icons::FinishIcons()
{
   // Update buffers
   p->UpdateBuffers();

   std::unique_lock lock {p->iconMutex_};

   // Swap buffers
   p->currentIconList_.swap(p->newValidIconList_);
   p->currentIconBuffer_.swap(p->newIconBuffer_);
   p->currentHoverIcons_.swap(p->newHoverIcons_);

   // Clear the new buffers, except the full icon list (used to update buffers
   // without re-adding icons)
   p->newValidIconList_.clear();
   p->newIconBuffer_.clear();
   p->newHoverIcons_.clear();

   // Mark the draw item dirty
   p->dirty_ = true;
}

void Icons::Impl::UpdateBuffers()
{
   newIconBuffer_.clear();
   newIconBuffer_.reserve(newIconList_.size() * kIconBufferLength);
   newValidIconList_.clear();
   newHoverIcons_.clear();

   for (auto& di : newIconList_)
   {
      auto it = currentIconSheets_.find(di->iconSheet_);
      if (it == currentIconSheets_.cend())
      {
         // No icon sheet found
         logger_->warn("Could not find icon sheet: {}", di->iconSheet_);
         continue;
      }

      auto& icon = it->second;

      // Validate icon
      if (di->iconIndex_ >= icon->numIcons_)
      {
         // No icon found
         logger_->warn("Invalid icon index: {}", di->iconIndex_);
         continue;
      }

      // Icon is valid, add to valid icon list
      newValidIconList_.push_back(di);

      // Base X/Y offsets in pixels
      const float x = static_cast<float>(di->x_);
      const float y = static_cast<float>(di->y_);

      // Icon size
      const float iw = static_cast<float>(icon->iconWidth_);
      const float ih = static_cast<float>(icon->iconHeight_);

      // Hot X/Y (zero-based icon center)
      const float hx = static_cast<float>(icon->hotX_);
      const float hy = static_cast<float>(icon->hotY_);

      // Final X/Y offsets in pixels
      const float lx = std::roundf(-hx);
      const float rx = std::roundf(lx + iw);
      const float ty = std::roundf(+hy);
      const float by = std::roundf(ty - ih);

      // Angle in degrees
      units::angle::degrees<float> angle = di->angle_;
      const float                  a     = angle.value();

      // Modulate color
      const float mc0 = di->modulate_[0];
      const float mc1 = di->modulate_[1];
      const float mc2 = di->modulate_[2];
      const float mc3 = di->modulate_[3];

      newIconBuffer_.insert(newIconBuffer_.end(),
                            {
                               // Icon
                               x, y, lx, by, mc0, mc1, mc2, mc3, a, // BL
                               x, y, lx, ty, mc0, mc1, mc2, mc3, a, // TL
                               x, y, rx, by, mc0, mc1, mc2, mc3, a, // BR
                               x, y, rx, by, mc0, mc1, mc2, mc3, a, // BR
                               x, y, rx, ty, mc0, mc1, mc2, mc3, a, // TR
                               x, y, lx, ty, mc0, mc1, mc2, mc3, a  // TL
                            });

      if (!di->hoverText_.empty() || di->event_ != nullptr)
      {
         const units::angle::radians<double> radians = angle;

         const float cosAngle = cosf(static_cast<float>(radians.value()));
         const float sinAngle = sinf(static_cast<float>(radians.value()));

         const glm::mat2 rotate {cosAngle, -sinAngle, sinAngle, cosAngle};

         const glm::vec2 otl = rotate * glm::vec2 {lx, ty};
         const glm::vec2 otr = rotate * glm::vec2 {rx, ty};
         const glm::vec2 obl = rotate * glm::vec2 {lx, by};
         const glm::vec2 obr = rotate * glm::vec2 {rx, by};

         newHoverIcons_.emplace_back(IconHoverEntry {di, otl, otr, obl, obr});
      }
   }
}

void Icons::Impl::UpdateTextureBuffer()
{
   textureBuffer_.clear();
   textureBuffer_.reserve(currentIconList_.size() * kTextureBufferLength);

   for (auto& di : currentIconList_)
   {
      auto it = currentIconSheets_.find(di->iconSheet_);
      if (it == currentIconSheets_.cend())
      {
         // No file found. Should not get here, but insert empty data to match
         // up with data already buffered
         logger_->error("Could not find icon sheet: {}", di->iconSheet_);

         // clang-format off
         textureBuffer_.insert(
            textureBuffer_.end(),
            {
               // Icon
               0.0f, 0.0f, 0.0f, // BL
               0.0f, 0.0f, 0.0f, // TL
               0.0f, 0.0f, 0.0f, // BR
               0.0f, 0.0f, 0.0f, // BR
               0.0f, 0.0f, 0.0f, // TR
               0.0f, 0.0f, 0.0f  // TL
            });
         // clang-format on

         continue;
      }

      auto& icon = it->second;

      // Validate icon
      if (di->iconIndex_ >= icon->numIcons_)
      {
         // No icon found
         logger_->error("Invalid icon index: {}", di->iconIndex_);

         // Will get here if a texture changes, and the texture shrunk such that
         // the icon is no longer found

         // clang-format off
         textureBuffer_.insert(
            textureBuffer_.end(),
            {
               // Icon
               0.0f, 0.0f, 0.0f, // BL
               0.0f, 0.0f, 0.0f, // TL
               0.0f, 0.0f, 0.0f, // BR
               0.0f, 0.0f, 0.0f, // BR
               0.0f, 0.0f, 0.0f, // TR
               0.0f, 0.0f, 0.0f  // TL
            });
         // clang-format on

         continue;
      }

      // Texture coordinates
      const std::size_t iconRow    = (di->iconIndex_) / icon->columns_;
      const std::size_t iconColumn = (di->iconIndex_) % icon->columns_;

      const float iconX = iconColumn * icon->scaledWidth_;
      const float iconY = iconRow * icon->scaledHeight_;

      const float ls = icon->texture_.sLeft_ + iconX;
      const float rs = ls + icon->scaledWidth_;
      const float tt = icon->texture_.tTop_ + iconY;
      const float bt = tt + icon->scaledHeight_;
      const float r  = static_cast<float>(icon->texture_.layerId_);

      // clang-format off
      textureBuffer_.insert(
         textureBuffer_.end(),
         {
            // Icon
            ls, bt, r, // BL
            ls, tt, r, // TL
            rs, bt, r, // BR
            rs, bt, r, // BR
            rs, tt, r, // TR
            ls, tt, r  // TL
         });
      // clang-format on
   }
}

void Icons::Impl::Update(bool textureAtlasChanged)
{
   gl::OpenGLFunctions& gl = context_->gl();

   // If the texture atlas has changed
   if (dirty_ || textureAtlasChanged || lastTextureAtlasChanged_)
   {
      // Update texture coordinates
      for (auto& iconSheet : currentIconSheets_)
      {
         iconSheet.second->UpdateTextureInfo();
      }

      // Update OpenGL texture buffer data
      UpdateTextureBuffer();

      // Buffer texture data
      gl.glBindBuffer(GL_ARRAY_BUFFER, vbo_[1]);
      gl.glBufferData(GL_ARRAY_BUFFER,
                      sizeof(float) * textureBuffer_.size(),
                      textureBuffer_.data(),
                      GL_DYNAMIC_DRAW);

      lastTextureAtlasChanged_ = false;
   }

   // If buffers need updating
   if (dirty_)
   {
      // Buffer vertex data
      gl.glBindBuffer(GL_ARRAY_BUFFER, vbo_[0]);
      gl.glBufferData(GL_ARRAY_BUFFER,
                      sizeof(float) * currentIconBuffer_.size(),
                      currentIconBuffer_.data(),
                      GL_DYNAMIC_DRAW);

      numVertices_ =
         static_cast<GLsizei>(currentIconBuffer_.size() / kPointsPerVertex);
   }

   dirty_ = false;
}

bool Icons::RunMousePicking(
   const QMapLibre::CustomLayerRenderParameters& params,
   const QPointF&                                mouseLocalPos,
   const QPointF&                                mouseGlobalPos,
   const glm::vec2& /* mouseCoords */,
   const common::Coordinate& /* mouseGeoCoords */,
   std::shared_ptr<types::EventHandler>& eventHandler)
{
   std::unique_lock lock {p->iconMutex_};

   bool itemPicked = false;

   // Convert local coordinates to icon coordinates
   glm::vec2 mouseLocalCoords {mouseLocalPos.x(),
                               params.height - mouseLocalPos.y()};

   // For each pickable icon
   auto it = std::find_if( //
      std::execution::par_unseq,
      p->currentHoverIcons_.crbegin(),
      p->currentHoverIcons_.crend(),
      [&mouseLocalCoords](const auto& icon)
      {
         // Initialize vertices
         glm::vec2 bl = {static_cast<float>(icon.di_->x_),
                         static_cast<float>(icon.di_->y_)};
         glm::vec2 br = bl;
         glm::vec2 tl = br;
         glm::vec2 tr = tl;

         // Offset vertices
         tl += icon.otl_;
         bl += icon.obl_;
         br += icon.obr_;
         tr += icon.otr_;

         // Test point against polygon bounds
         return util::maplibre::IsPointInPolygon({tl, bl, br, tr},
                                                 mouseLocalCoords);
      });

   if (it != p->currentHoverIcons_.crend())
   {
      itemPicked = true;

      if (!it->di_->hoverText_.empty())
      {
         // Show tooltip
         util::tooltip::Show(it->di_->hoverText_, mouseGlobalPos);
      }
      if (it->di_->event_ != nullptr)
      {
         // Register event handler
         eventHandler = it->di_;
      }
   }

   return itemPicked;
}

void Icons::RegisterEventHandler(
   const std::shared_ptr<IconDrawItem>& di,
   const std::function<void(QEvent*)>&  eventHandler)
{
   di->event_ = eventHandler;
}

} // namespace draw
} // namespace gl
} // namespace qt
} // namespace scwx
