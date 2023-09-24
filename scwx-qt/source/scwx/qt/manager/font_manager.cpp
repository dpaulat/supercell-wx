#include <scwx/qt/manager/font_manager.hpp>
#include <scwx/util/logger.hpp>

namespace scwx
{
namespace qt
{
namespace manager
{

static const std::string logPrefix_ = "scwx::qt::manager::font_manager";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class FontManager::Impl
{
public:
   explicit Impl() {}
   ~Impl() {}
};

FontManager::FontManager() : p(std::make_unique<Impl>()) {}

FontManager::~FontManager() {};

std::shared_ptr<FontManager> FontManager::Instance()
{
   static std::weak_ptr<FontManager> fontManagerReference_ {};
   static std::mutex                 instanceMutex_ {};

   std::unique_lock lock(instanceMutex_);

   std::shared_ptr<FontManager> fontManager = fontManagerReference_.lock();

   if (fontManager == nullptr)
   {
      fontManager           = std::make_shared<FontManager>();
      fontManagerReference_ = fontManager;
   }

   return fontManager;
}

} // namespace manager
} // namespace qt
} // namespace scwx
