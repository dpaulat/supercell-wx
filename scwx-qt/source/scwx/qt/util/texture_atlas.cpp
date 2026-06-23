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

#include <boost/gil/extension/io/bmp.hpp>
#include <boost/gil/extension/io/jpeg.hpp>
#include <boost/gil/extension/io/png.hpp>
#include <boost/gil/extension/io/targa.hpp>
#include <boost/gil/extension/io/tiff.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/timer/timer.hpp>
#include <cpr/cpr.h>
#include <stb_image.h>
#include <stb_rect_pack.h>
#include <QFile>
#include <QFileInfo>
#include <QPainter>
#include <QSvgRenderer>
#include <QUrl>

#if defined(_MSC_VER)
#   pragma warning(pop)
#endif

#if defined(LoadImage)
#   undef LoadImage
#endif

namespace scwx::qt::util
{

static const std::string logPrefix_ = "scwx::qt::util::texture_atlas";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static const boost::gil::rgba8_pixel_t kMagenta_ {255, 0, 255, 255};

class TextureAtlas::Impl
{
public:
   explicit Impl() = default;
   ~Impl()         = default;

   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   static std::shared_ptr<boost::gil::rgba8_image_t>
   LoadImage(const std::string& imagePath, double scale = 1);

   template<typename Tag>
   static std::shared_ptr<boost::gil::rgba8_image_t>
   ReadImageFile(const QString& imagePath);
   static std::shared_ptr<boost::gil::rgba8_image_t>
   ReadSvgFile(const QString& imagePath, double scale = 1);

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

std::shared_ptr<boost::gil::rgba8_image_t> TextureAtlas::CacheTexture(
   const std::string& name, const std::string& path, double scale)
{
   // Attempt to load the image
   std::shared_ptr<boost::gil::rgba8_image_t> image =
      TextureAtlas::Impl::LoadImage(path, scale);

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

   using ImageVector = std::vector<
      std::pair<std::string, std::shared_ptr<boost::gil::rgba8_image_t>>>;

   ImageVector             images {};
   std::vector<stbrp_rect> stbrpRects {};

   // Padding in pixels around each image in the atlas. This prevents
   // Linear sampling can bleed into neighboring images. Use 1 px
   // padding by default. If an image equals the atlas size, padding is
   // skipped for that image.
   const int pad = 1;

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
            // Increase requested rect size by padding on all sides unless
            // the image is as large as the atlas dimension, in which case
            // we request the exact size (no padding possible).
            // Only add padding if the padded image will still fit in the
            // atlas dimensions.
            const std::size_t paddedW = image->width() + 2LL * pad;
            const std::size_t paddedH = image->height() + 2LL * pad;

            const auto reqW = static_cast<stbrp_coord>(
               (paddedW <= width) ? paddedW : image->width());
            const auto reqH = static_cast<stbrp_coord>(
               (paddedH <= height) ? paddedH : image->height());

            stbrpRects.push_back(stbrp_rect {0, reqW, reqH, 0, 0, 0});

            // Store image data in a vector
            images.push_back({texture.first, image});
         }

         // Increment iterator
         ++it;
      }
   }

   // Texture array layer support is guaranteed to handle at least 256 layers.
   constexpr std::size_t kMaxLayers = 256u;

   const float xStep = 1.0f / static_cast<float>(width);
   const float yStep = 1.0f / static_cast<float>(height);
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
      boost::gil::rgba8_image_t atlas(
         static_cast<boost::gil::rgba8_image_t::x_coord_t>(width),
         static_cast<boost::gil::rgba8_image_t::y_coord_t>(height));

      // NOLINTNEXTLINE(misc-const-correctness): Image is modified
      boost::gil::rgba8_view_t atlasView = boost::gil::view(atlas);
      boost::gil::fill_pixels(atlasView, kMagenta_);

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

            const int packedX = stbrpRects[i].x;
            const int packedY = stbrpRects[i].y;

            // Recompute padded sizes for this image (padded values were
            // local to the earlier loop). This determines whether padding
            // was requested for the image.
            const std::size_t paddedW = imageView.width() + 2LL * pad;
            const std::size_t paddedH = imageView.height() + 2LL * pad;

            // Determine whether padding was requested/used for this image.
            const bool usedPadding =
               ((static_cast<std::size_t>(stbrpRects[i].w) == paddedW) &&
                (static_cast<std::size_t>(stbrpRects[i].h) == paddedH));

            if (usedPadding)
            {
               // Create a subview for the inner region where the image will
               // be copied (offset by pad).
               // NOLINTNEXTLINE(misc-const-correctness): Image is modified
               boost::gil::rgba8_view_t atlasInnerView =
                  boost::gil::subimage_view(atlasView,
                                            packedX + pad,
                                            packedY + pad,
                                            imageView.width(),
                                            imageView.height());

               // Copy image pixels into inner region
               boost::gil::copy_pixels(imageView, atlasInnerView);

               // Replicate left/right edges into padding
               for (int yy = 0; yy < static_cast<int>(imageView.height()); ++yy)
               {
                  const auto& leftPixel =
                     atlasView(packedX + pad, packedY + pad + yy);
                  const auto& rightPixel = atlasView(
                     packedX + pad + static_cast<int>(imageView.width()) - 1,
                     packedY + pad + yy);

                  for (int px = 0; px < pad; ++px)
                  {
                     atlasView(packedX + px, packedY + pad + yy) = leftPixel;
                     atlasView(packedX + pad +
                                  static_cast<int>(imageView.width()) + px,
                               packedY + pad + yy)               = rightPixel;
                  }
               }

               // Replicate top/bottom rows (including padded columns)
               for (int xx = 0;
                    xx < static_cast<int>(imageView.width()) + 2 * pad;
                    ++xx)
               {
                  const auto& topPixel = atlasView(packedX + xx, packedY + pad);
                  const auto& bottomPixel = atlasView(
                     packedX + xx,
                     packedY + pad + static_cast<int>(imageView.height()) - 1);

                  for (int py = 0; py < pad; ++py)
                  {
                     atlasView(packedX + xx, packedY + py) = topPixel;
                     atlasView(packedX + xx,
                               packedY + pad +
                                  static_cast<int>(imageView.height()) + py) =
                        bottomPixel;
                  }
               }
            }
            else
            {
               // No padding used (image may be same size as atlas). Copy as-is
               // NOLINTNEXTLINE(misc-const-correctness): Image is modified
               boost::gil::rgba8_view_t atlasSubView =
                  boost::gil::subimage_view(atlasView,
                                            packedX,
                                            packedY,
                                            imageView.width(),
                                            imageView.height());

               boost::gil::copy_pixels(imageView, atlasSubView);
            }

            // Add texture image to the index
            const stbrp_coord x = stbrpRects[i].x;
            const stbrp_coord y = stbrpRects[i].y;

            // If padding was used, the actual image starts at (x+pad,y+pad)
            // within the packed rectangle. If not, it starts at (x,y).
            const auto imgX = static_cast<float>((usedPadding) ? (x + pad) : x);
            const auto imgY = static_cast<float>((usedPadding) ? (y + pad) : y);

            const float sLeft = imgX * xStep + xMin;
            const float sRight =
               sLeft + static_cast<float>(imageView.width() - 1) /
                          static_cast<float>(width);
            const float tTop = imgY * yStep + yMin;
            const float tBottom =
               tTop + static_cast<float>(imageView.height() - 1) /
                         static_cast<float>(height);

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

std::size_t TextureAtlas::LayerCount() const
{
   std::shared_lock lock(p->atlasMutex_);
   return p->atlasArray_.size();
}

std::size_t TextureAtlas::AtlasWidth() const
{
   std::shared_lock lock(p->atlasMutex_);

   if (p->atlasArray_.empty())
   {
      return 0;
   }

   return p->atlasArray_[0].width();
}

std::size_t TextureAtlas::AtlasHeight() const
{
   std::shared_lock lock(p->atlasMutex_);

   if (p->atlasArray_.empty())
   {
      return 0;
   }

   return p->atlasArray_[0].height();
}

const std::uint8_t* TextureAtlas::LayerPixels(std::size_t  layer,
                                              std::size_t& byteSize) const
{
   std::shared_lock lock(p->atlasMutex_);

   if (layer >= p->atlasArray_.size())
   {
      byteSize = 0;
      return nullptr;
   }

   const boost::gil::rgba8_image_t& image = p->atlasArray_[layer];
   const boost::gil::rgba8c_view_t  view  = boost::gil::const_view(image);
   byteSize =
      image.width() * image.height() * sizeof(boost::gil::rgba8_pixel_t);
   return reinterpret_cast<const std::uint8_t*>(&view(0, 0));
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
TextureAtlas::Impl::LoadImage(const std::string& imagePath, double scale)
{
   logger_->debug("Loading image: {}", imagePath);

   std::shared_ptr<boost::gil::rgba8_image_t> image = nullptr;

   QString qImagePath = QString::fromStdString(imagePath);

   QUrl url = QUrl::fromUserInput(qImagePath);

   if (url.isLocalFile())
   {
      const QString suffix          = QFileInfo(qImagePath).suffix().toLower();
      const QString qLocalImagePath = url.toString(QUrl::PreferLocalFile);

      static const std::unordered_map<
         QString,
         std::function<std::shared_ptr<boost::gil::rgba8_image_t>(
            const QString&, double)>>
         formatHandlers = {{"bmp",
                            [](const QString& path, double)
                            {
                               return ReadImageFile<boost::gil::bmp_tag>(path);
                            }},
                           {"jpg",
                            [](const QString& path, double)
                            {
                               return ReadImageFile<boost::gil::jpeg_tag>(path);
                            }},
                           {"jpeg",
                            [](const QString& path, double)
                            {
                               return ReadImageFile<boost::gil::jpeg_tag>(path);
                            }},
                           {"png",
                            [](const QString& path, double)
                            {
                               return ReadImageFile<boost::gil::png_tag>(path);
                            }},
                           {"svg",
                            [](const QString& path, double scale)
                            {
                               return ReadSvgFile(path, scale);
                            }},
                           {"tga",
                            [](const QString& path, double)
                            {
                               return ReadImageFile<boost::gil::targa_tag>(
                                  path);
                            }},
                           {"tif",
                            [](const QString& path, double)
                            {
                               return ReadImageFile<boost::gil::tiff_tag>(path);
                            }},
                           {"tiff",
                            [](const QString& path, double)
                            {
                               return ReadImageFile<boost::gil::tiff_tag>(path);
                            }}};

      const auto it = formatHandlers.find(suffix);
      if (it != formatHandlers.end())
      {
         image = it->second(qLocalImagePath, scale);
      }
      else
      {
         // Default to PNG for unknown formats
         image = ReadImageFile<boost::gil::png_tag>(qLocalImagePath);
      }
   }
   else
   {
      auto response = cpr::Get(cpr::Url {imagePath},
                               network::cpr::GetHeader(),
                               network::cpr::GetDefaultTimeout(),
                               network::cpr::GetDefaultConnectTimeout(),
                               network::cpr::GetDefaultLowSpeed());

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
         image      = std::make_shared<boost::gil::rgba8_image_t>();
         *image     = boost::gil::rgba8_image_t(stbView);
         auto& view = boost::gil::view(*image);

         // If no alpha channel, replace black with transparent
         if (numChannels == 3)
         {
            std::for_each(std::execution::par,
                          view.begin(),
                          view.end(),
                          [](boost::gil::rgba8_pixel_t& pixel)
                          {
                             static const boost::gil::rgba8_pixel_t kBlack {
                                0, 0, 0, 255};
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

template<typename Tag>
std::shared_ptr<boost::gil::rgba8_image_t>
TextureAtlas::Impl::ReadImageFile(const QString& imagePath)
{
   QFile imageFile(imagePath);

   const bool isOpen = imageFile.open(QIODevice::ReadOnly);

   if (!isOpen || !imageFile.isOpen())
   {
      logger_->error("Could not open image: {}", imagePath.toStdString());
      return nullptr;
   }

   boost::iostreams::stream<util::IoDeviceSource> dataStream(imageFile);
   std::shared_ptr<boost::gil::rgba8_image_t>     image =
      std::make_shared<boost::gil::rgba8_image_t>();

   try
   {
      boost::gil::read_and_convert_image(dataStream, *image, Tag());
   }
   catch (const std::exception& ex)
   {
      logger_->error(
         "Error reading image ({}): {}", imagePath.toStdString(), ex.what());
      return nullptr;
   }

   return image;
}

std::shared_ptr<boost::gil::rgba8_image_t>
TextureAtlas::Impl::ReadSvgFile(const QString& imagePath, double scale)
{
   QSvgRenderer renderer {imagePath};
   QPixmap      pixmap {renderer.defaultSize() * scale};
   pixmap.fill(Qt::GlobalColor::transparent);

   QPainter painter {&pixmap};
   renderer.render(&painter, pixmap.rect());

   QImage qImage = pixmap.toImage();

   std::shared_ptr<boost::gil::rgba8_image_t> image = nullptr;

   if (qImage.width() > 0 && qImage.height() > 0)
   {
      // Convert to ARGB32 format if not already (equivalent to bgra8_pixel_t)
      qImage.convertTo(QImage::Format_ARGB32);

      // Create a view pointing to the underlying QImage pixel data
      auto view = boost::gil::interleaved_view(
         qImage.width(),
         qImage.height(),
         reinterpret_cast<const boost::gil::bgra8_pixel_t*>(qImage.constBits()),
         qImage.width() * 4);

      image = std::make_shared<boost::gil::rgba8_image_t>(view);
   }

   return image;
}

TextureAtlas& TextureAtlas::Instance()
{
   static TextureAtlas instance_ {};
   return instance_;
}

} // namespace scwx::qt::util
