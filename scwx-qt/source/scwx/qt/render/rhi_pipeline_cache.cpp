#include <scwx/qt/main/application_paths.hpp>
#include <scwx/qt/render/rhi_pipeline_cache.hpp>
#if !defined(__APPLE__)
#   include <scwx/qt/render/rhi_vulkan_result.hpp>
#endif
#include <scwx/util/environment.hpp>
#include <scwx/util/logger.hpp>

#include <cstring>

#include <QDataStream>
#include <QFile>
#include <QSaveFile>

#include <rhi/qrhi.h>

namespace scwx::qt::render
{

namespace
{

static const std::string logPrefix_ = "scwx::qt::render::rhi_pipeline_cache";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

constexpr char    kMagic[]         = {'S', 'C', 'W', 'X', 'P', 'C', '1'};
constexpr quint32 kCacheVersion    = 4;
constexpr char    kQrhiCacheFile[] = "qrhi-vulkan-pipeline-cache.bin";

bool PipelineCacheDisabled()
{
   return scwx::util::HasEnvironment("SCWX_VULKAN_PIPELINE_CACHE_DISABLE");
}

std::filesystem::path CacheFilePath(const char* fileName)
{
   return main::ApplicationPaths::GetLocation(
             main::ApplicationPaths::StandardLocation::Cache) /
          fileName;
}

bool ReadCacheFile(const std::filesystem::path& path, QByteArray& data)
{
   QFile file(QString::fromStdString(path.string()));
   if (!file.open(QIODevice::ReadOnly))
   {
      return false;
   }

   QDataStream stream(&file);
   stream.setVersion(QDataStream::Qt_6_0);

   char magic[sizeof(kMagic)] {};
   if (stream.readRawData(magic, sizeof(magic)) != sizeof(magic) ||
       std::memcmp(magic, kMagic, sizeof(kMagic)) != 0)
   {
      return false;
   }

   quint32 version = 0;
   stream >> version;
   if (version != kCacheVersion || stream.status() != QDataStream::Ok)
   {
      return false;
   }

   stream >> data;
   return stream.status() == QDataStream::Ok && !data.isEmpty();
}

bool WriteCacheFile(const std::filesystem::path& path, const QByteArray& data)
{
   if (data.isEmpty())
   {
      return false;
   }

   QSaveFile file(QString::fromStdString(path.string()));
   if (!file.open(QIODevice::WriteOnly))
   {
      logger_->warn("Unable to open pipeline cache for write: {}",
                    path.string());
      return false;
   }

   QDataStream stream(&file);
   stream.setVersion(QDataStream::Qt_6_0);
   stream.writeRawData(kMagic, sizeof(kMagic));
   stream << kCacheVersion;
   stream << data;

   if (stream.status() != QDataStream::Ok || !file.commit())
   {
      logger_->warn("Unable to write pipeline cache: {}", path.string());
      return false;
   }

   return true;
}

} // namespace

void RestoreQrhiPipelineCache(QRhi* rhi)
{
   if (PipelineCacheDisabled() || rhi == nullptr ||
       rhi->backend() != QRhi::Vulkan)
   {
      return;
   }

   if (!rhi->isFeatureSupported(QRhi::PipelineCacheDataLoadSave))
   {
      logger_->debug("QRhi pipeline cache load/save not supported");
      return;
   }

   QByteArray data;
   if (!ReadCacheFile(CacheFilePath(kQrhiCacheFile), data))
   {
      return;
   }

   rhi->setPipelineCacheData(data);
   logger_->info("Loaded QRhi Vulkan pipeline cache ({} bytes)", data.size());
}

void PersistQrhiPipelineCache(QRhi* rhi)
{
   if (PipelineCacheDisabled() || rhi == nullptr ||
       rhi->backend() != QRhi::Vulkan)
   {
      return;
   }

   if (!rhi->isFeatureSupported(QRhi::PipelineCacheDataLoadSave))
   {
      return;
   }

   const QByteArray data = rhi->pipelineCacheData();
   if (data.isEmpty())
   {
      logger_->debug("QRhi pipeline cache empty; skip persist");
      return;
   }

   if (WriteCacheFile(CacheFilePath(kQrhiCacheFile), data))
   {
      logger_->info("Saved QRhi Vulkan pipeline cache ({} bytes)", data.size());
   }
}

#if !defined(__APPLE__)

QByteArray LoadVulkanPipelineCacheBlob(const char* fileName)
{
   QByteArray data;
   if (PipelineCacheDisabled() || fileName == nullptr)
   {
      return data;
   }

   ReadCacheFile(CacheFilePath(fileName), data);
   return data;
}

void SaveVulkanPipelineCacheBlob(const char* fileName, const QByteArray& data)
{
   if (PipelineCacheDisabled() || fileName == nullptr || data.isEmpty())
   {
      return;
   }

   if (WriteCacheFile(CacheFilePath(fileName), data))
   {
      logger_->info(
         "Saved Vulkan pipeline cache {} ({} bytes)", fileName, data.size());
   }
}

VkResult CreateVulkanPipelineCache(VkDevice          device,
                                   const QByteArray& initialData,
                                   VkPipelineCache*  pipelineCache)
{
   if (device == VK_NULL_HANDLE || pipelineCache == nullptr)
   {
      return VK_ERROR_INITIALIZATION_FAILED;
   }

   VkPipelineCacheCreateInfo createInfo {};
   createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
   if (!initialData.isEmpty())
   {
      createInfo.initialDataSize = static_cast<std::size_t>(initialData.size());
      createInfo.pInitialData    = initialData.constData();
   }

   const VkResult result =
      vkCreatePipelineCache(device, &createInfo, nullptr, pipelineCache);
   ReportVulkanResult(result, "vkCreatePipelineCache");
   return result;
}

QByteArray GetVulkanPipelineCacheBlob(VkDevice        device,
                                      VkPipelineCache pipelineCache)
{
   QByteArray data;
   if (device == VK_NULL_HANDLE || pipelineCache == VK_NULL_HANDLE)
   {
      return data;
   }

   std::size_t dataSize = 0;
   VkResult    result =
      vkGetPipelineCacheData(device, pipelineCache, &dataSize, nullptr);
   if (result != VK_SUCCESS || dataSize == 0)
   {
      return data;
   }

   data.resize(static_cast<int>(dataSize));
   result =
      vkGetPipelineCacheData(device, pipelineCache, &dataSize, data.data());
   ReportVulkanResult(result, "vkGetPipelineCacheData");
   if (result != VK_SUCCESS)
   {
      data.clear();
      return data;
   }

   data.resize(static_cast<int>(dataSize));
   return data;
}

#endif // !defined(__APPLE__)

} // namespace scwx::qt::render
