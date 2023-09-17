#include <scwx/qt/util/texture_atlas.hpp>
#include <scwx/qt/util/streams.hpp>
#include <scwx/network/cpr.hpp>
#include <scwx/util/logger.hpp>

#include <execution>
#include <shared_mutex>
#include <unordered_map>

#if defined(_MSC_VER)
#   pragma warning(push, 0)
#   pragma warning(disable : 4702)
#   pragma warning(disable : 4714)
#endif

#include <boost/gil/extension/io/png.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/timer/timer.hpp>
#include <cpr/cpr.h>
#include <stb_image.h>
#include <stb_rect_pack.h>
#include <QFile>
#include <QUrl>

#if defined(_MSC_VER)
#   pragma warning(pop)
#endif

#if defined(LoadImage)
#   undef LoadImage
#endif

namespace scwx
{
namespace qt
{
namespace util
{

static const std::string logPrefix_ = "scwx::qt::util::texture_atlas";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class TextureAtlas::Impl
{
public:
   explicit Impl() {}
   ~Impl() {}

   static std::shared_ptr<boost::gil::rgba8_image_t>
   LoadImage(const std::string& imagePath);

   std::vector<std::shared_ptr<boost::gil::rgba8_image_t>>
                     registeredTextures_ {};
   std::shared_mutex registeredTextureMutex_ {};

   std::shared_mutex textureCacheMutex_ {};
   std::unordered_map<std::string, std::weak_ptr<boost::gil::rgba8_image_t>>
      textureCache_ {};

   std::vector<boost::gil::rgba8_image_t>             atlasArray_ {};
   std::unordered_map<std::string, TextureAttributes> atlasMap_ {};
   std::shared_mutex                                  atlasMutex_ {};

   std::uint64_t buildCount_ {0u};
};

TextureAtlas::TextureAtlas() : p(std::make_unique<Impl>()) {}
TextureAtlas::~TextureAtlas() = default;

TextureAtlas::TextureAtlas(TextureAtlas&&) noexcept            = default;
TextureAtlas& TextureAtlas::operator=(TextureAtlas&&) noexcept = default;

std::uint64_t TextureAtlas::BuildCount() const
{
   return p->buildCount_;
}

void TextureAtlas::RegisterTexture(const std::string& name,
                                   const std::string& path)
{
   std::unique_lock lock(p->registeredTextureMutex_);

   std::shared_ptr<boost::gil::rgba8_image_t> image = CacheTexture(name, path);
   p->registeredTextures_.emplace_back(std::move(image));
}

std::shared_ptr<boost::gil::rgba8_image_t>
TextureAtlas::CacheTexture(const std::string& name, const std::string& path)
{
   // Attempt to load the image
   std::shared_ptr<boost::gil::rgba8_image_t> image =
      TextureAtlas::Impl::LoadImage(path);

   // If the image is valid
   if (image != nullptr && image->width() > 0 && image->height() > 0)
   {
      // Store it in the texture cache
      std::unique_lock lock(p->textureCacheMutex_);

      p->textureCache_.insert_or_assign(name, image);

      return image;
   }

   return nullptr;
}

void TextureAtlas::BuildAtlas(std::size_t width, std::size_t height)
{
   logger_->debug("Building {}x{} texture atlas", width, height);

   boost::timer::cpu_timer timer {};
   timer.start();

   if (width > INT_MAX || height > INT_MAX)
   {
      logger_->error("Cannot build texture atlas of size {}x{}", width, height);
      return;
   }

   typedef std::vector<
      std::pair<std::string, std::shared_ptr<boost::gil::rgba8_image_t>>>
      ImageVector;

   ImageVector             images {};
   std::vector<stbrp_rect> stbrpRects {};

   // Cached images
   {
      // Take a lock on the texture cache map while adding textures images to
      // the atlas vector.
      std::unique_lock textureCacheLock(p->textureCacheMutex_);

      // For each cached texture
      for (auto it = p->textureCache_.begin(); it != p->textureCache_.end();)
      {
         auto& texture = *it;
         auto  image   = texture.second.lock();

         if (image == nullptr)
         {
            logger_->trace("Removing texture from the cache: {}",
                           texture.first);

            // If the image is no longer cached, erase the iterator and continue
            it = p->textureCache_.erase(it);
            continue;
         }
         else if (image->width() > 0u && image->height() > 0u)
         {
            // Store STB rectangle pack data in a vector
            stbrpRects.push_back(
               stbrp_rect {0,
                           static_cast<stbrp_coord>(image->width()),
                           static_cast<stbrp_coord>(image->height()),
                           0,
                           0,
                           0});

            // Store image data in a vector
            images.push_back({texture.first, image});
         }

         // Increment iterator
         ++it;
      }
   }

   // GL_MAX_ARRAY_TEXTURE_LAYERS is guaranteed to be at least 256 in OpenGL 3.3
   constexpr std::size_t kMaxLayers = 256u;

   const float xStep = 1.0f / width;
   const float yStep = 1.0f / height;
   const float xMin  = xStep * 0.5f;
   const float yMin  = yStep * 0.5f;

   // Optimal number of nodes = width
   stbrp_context           stbrpContext;
   std::vector<stbrp_node> stbrpNodes(width);
   ImageVector             unpackedImages {};
   std::vector<stbrp_rect> unpackedRects {};

   std::vector<boost::gil::rgba8_image_t>             newAtlasArray {};
   std::unordered_map<std::string, TextureAttributes> newAtlasMap {};

   for (std::size_t layer = 0; layer < kMaxLayers; ++layer)
   {
      logger_->trace("Processing layer {}", layer);

      // Pack images
      {
         logger_->trace("Packing {} images", images.size());

         stbrp_init_target(&stbrpContext,
                           static_cast<int>(width),
                           static_cast<int>(height),
                           stbrpNodes.data(),
                           static_cast<int>(stbrpNodes.size()));

         // Pack loaded textures
         stbrp_pack_rects(&stbrpContext,
                          stbrpRects.data(),
                          static_cast<int>(stbrpRects.size()));
      }

      // Clear atlas
      boost::gil::rgba8_image_t atlas(width, height);
      boost::gil::rgba8_view_t  atlasView = boost::gil::view(atlas);
      boost::gil::fill_pixels(atlasView,
                              boost::gil::rgba8_pixel_t {255, 0, 255, 255});

      // Populate atlas
      logger_->trace("Populating atlas");

      std::size_t numPackedImages = 0u;

      for (std::size_t i = 0; i < images.size(); ++i)
      {
         // If the image was packed successfully
         if (stbrpRects[i].was_packed != 0)
         {
            // Populate the atlas
            boost::gil::rgba8c_view_t imageView =
               boost::gil::const_view(*images[i].second);

            boost::gil::rgba8_view_t atlasSubView =
               boost::gil::subimage_view(atlasView,
                                         stbrpRects[i].x,
                                         stbrpRects[i].y,
                                         imageView.width(),
                                         imageView.height());

            boost::gil::copy_pixels(imageView, atlasSubView);

            // Add texture image to the index
            const stbrp_coord x = stbrpRects[i].x;
            const stbrp_coord y = stbrpRects[i].y;

            const float sLeft = x * xStep + xMin;
            const float sRight =
               sLeft + static_cast<float>(imageView.width() - 1) / width;
            const float tTop = y * yStep + yMin;
            const float tBottom =
               tTop + static_cast<float>(imageView.height() - 1) / height;

            newAtlasMap.emplace(
               std::piecewise_construct,
               std::forward_as_tuple(images[i].first),
               std::forward_as_tuple(
                  layer,
                  boost::gil::point_t {x, y},
                  boost::gil::point_t {imageView.width(), imageView.height()},
                  sLeft,
                  sRight,
                  tTop,
                  tBottom));

            numPackedImages++;
         }
         else
         {
            unpackedImages.push_back(std::move(images[i]));
            unpackedRects.push_back(stbrpRects[i]);
         }
      }

      if (numPackedImages > 0u)
      {
         // The new atlas layer has images that were able to be packed
         newAtlasArray.emplace_back(std::move(atlas));
      }

      if (unpackedImages.empty())
      {
         // All images have been packed into the texture atlas
         break;
      }
      else if (layer == kMaxLayers - 1u || numPackedImages == 0u)
      {
         // Some images were unable to be packed into the texture atlas
         for (auto& image : unpackedImages)
         {
            logger_->warn("Unable to pack texture: {}", image.first);
         }
      }
      else
      {
         // Swap in unpacked images for processing the next atlas layer
         images.swap(unpackedImages);
         stbrpRects.swap(unpackedRects);
         unpackedImages.clear();
         unpackedRects.clear();
      }
   }

   // Lock atlas
   std::unique_lock lock(p->atlasMutex_);

   p->atlasArray_.swap(newAtlasArray);
   p->atlasMap_.swap(newAtlasMap);

   // Mark the need to buffer the atlas
   ++p->buildCount_;

   timer.stop();
   logger_->debug("Texture atlas built in {}", timer.format(6, "%ws"));
}

void TextureAtlas::BufferAtlas(gl::OpenGLFunctions& gl, GLuint texture)
{
   std::shared_lock lock(p->atlasMutex_);

   if (p->atlasArray_.size() > 0u && p->atlasArray_[0].width() > 0 &&
       p->atlasArray_[0].height() > 0)
   {
      const std::size_t numLayers = p->atlasArray_.size();
      const std::size_t width     = p->atlasArray_[0].width();
      const std::size_t height    = p->atlasArray_[0].height();
      const std::size_t layerSize = width * height;

      std::vector<boost::gil::rgba8_pixel_t> pixelData {layerSize * numLayers};

      for (std::size_t i = 0; i < numLayers; ++i)
      {
         boost::gil::rgba8_view_t view = boost::gil::view(p->atlasArray_[i]);

         boost::gil::copy_pixels(
            view,
            boost::gil::interleaved_view(view.width(),
                                         view.height(),
                                         pixelData.data() + (i * layerSize),
                                         view.width() *
                                            sizeof(boost::gil::rgba8_pixel_t)));
      }

      lock.unlock();

      gl.glBindTexture(GL_TEXTURE_2D_ARRAY, texture);

      gl.glTexParameteri(
         GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      gl.glTexParameteri(
         GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      gl.glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      gl.glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

      gl.glTexImage3D(GL_TEXTURE_2D_ARRAY,
                      0,
                      GL_RGBA,
                      static_cast<GLsizei>(width),
                      static_cast<GLsizei>(height),
                      static_cast<GLsizei>(numLayers),
                      0,
                      GL_RGBA,
                      GL_UNSIGNED_BYTE,
                      pixelData.data());
   }
}

TextureAttributes TextureAtlas::GetTextureAttributes(const std::string& name)
{
   TextureAttributes attr {};
   std::shared_lock  lock(p->atlasMutex_);

   const auto& it = p->atlasMap_.find(name);
   if (it != p->atlasMap_.cend())
   {
      attr = it->second;
   }

   return attr;
}

std::shared_ptr<boost::gil::rgba8_image_t>
TextureAtlas::Impl::LoadImage(const std::string& imagePath)
{
   logger_->debug("Loading image: {}", imagePath);

   std::shared_ptr<boost::gil::rgba8_image_t> image =
      std::make_shared<boost::gil::rgba8_image_t>();

   QUrl url = QUrl::fromUserInput(QString::fromStdString(imagePath));

   if (url.isLocalFile())
   {
      QFile imageFile(imagePath.c_str());

      imageFile.open(QIODevice::ReadOnly);

      if (!imageFile.isOpen())
      {
         logger_->error("Could not open image: {}", imagePath);
         return nullptr;
      }

      boost::iostreams::stream<util::IoDeviceSource> dataStream(imageFile);

      try
      {
         boost::gil::read_and_convert_image(
            dataStream, *image, boost::gil::png_tag());
      }
      catch (const std::exception& ex)
      {
         logger_->error("Error reading image: {}", ex.what());
         return nullptr;
      }
   }
   else
   {
      auto response = cpr::Get(cpr::Url {imagePath}, network::cpr::GetHeader());

      if (cpr::status::is_success(response.status_code))
      {
         // Use stbi, since we can only guess the image format
         static constexpr int desiredChannels = 4;

         int width;
         int height;
         int numChannels;

         unsigned char* pixelData = stbi_load_from_memory(
            reinterpret_cast<const unsigned char*>(response.text.data()),
            static_cast<int>(
               std::clamp<std::size_t>(response.text.size(), 0, INT32_MAX)),
            &width,
            &height,
            &numChannels,
            desiredChannels);

         if (pixelData == nullptr)
         {
            logger_->error("Error loading image: {}", stbi_failure_reason());
            return nullptr;
         }

         // Create a view pointing to the STB image data
         auto stbView = boost::gil::interleaved_view(
            width,
            height,
            reinterpret_cast<boost::gil::rgba8_pixel_t*>(pixelData),
            width * desiredChannels);

         // Copy the view to the destination image
         *image     = boost::gil::rgba8_image_t(stbView);
         auto& view = boost::gil::view(*image);

         // If no alpha channel, replace black with transparent
         if (numChannels == 3)
         {
            std::for_each(
               std::execution::par_unseq,
               view.begin(),
               view.end(),
               [](boost::gil::rgba8_pixel_t& pixel)
               {
                  static const boost::gil::rgba8_pixel_t kBlack {0, 0, 0, 255};
                  if (pixel == kBlack)
                  {
                     pixel[3] = 0;
                  }
               });
         }

         stbi_image_free(pixelData);
      }
      else if (response.status_code == 0)
      {
         logger_->error("Error loading image: {}", response.error.message);
      }
      else
      {
         logger_->error("Error loading image: {}", response.status_line);
      }
   }

   return image;
}

TextureAtlas& TextureAtlas::Instance()
{
   static TextureAtlas instance_ {};
   return instance_;
}

} // namespace util
} // namespace qt
} // namespace scwx
