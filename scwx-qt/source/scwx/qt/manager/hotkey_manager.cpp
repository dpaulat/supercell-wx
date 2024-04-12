#include <scwx/qt/manager/hotkey_manager.hpp>
#include <scwx/qt/settings/hotkey_settings.hpp>
#include <scwx/util/logger.hpp>

#include <vector>

#include <boost/container/flat_map.hpp>
#include <QKeyEvent>
#include <QKeySequence>

namespace scwx
{
namespace qt
{
namespace manager
{

static const std::string logPrefix_ = "scwx::qt::manager::hotkey_manager";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class HotkeyManager::Impl
{
public:
   explicit Impl()
   {
      auto& hotkeySettings = settings::HotkeySettings::Instance();

      for (auto hotkey : types::HotkeyIterator())
      {
         auto& hotkeyVariable = hotkeySettings.hotkey(hotkey);

         UpdateHotkey(hotkey, hotkeyVariable.GetValue());

         callbacks_.emplace_back(hotkeyVariable,
                                 hotkeyVariable.RegisterValueChangedCallback(
                                    [this, hotkey](const std::string& value)
                                    { UpdateHotkey(hotkey, value); }));
      }
   }

   ~Impl()
   {
      for (auto& callback : callbacks_)
      {
         callback.first.UnregisterValueChangedCallback(callback.second);
      }
   }

   void UpdateHotkey(types::Hotkey hotkey, const std::string& value);

   std::vector<
      std::pair<settings::SettingsVariable<std::string>&, boost::uuids::uuid>>
                                                           callbacks_ {};
   boost::container::flat_map<types::Hotkey, QKeySequence> hotkeys_ {};
};

HotkeyManager::HotkeyManager() : p(std::make_unique<Impl>()) {}
HotkeyManager::~HotkeyManager() = default;

void HotkeyManager::Impl::UpdateHotkey(types::Hotkey      hotkey,
                                       const std::string& value)
{
   hotkeys_.insert_or_assign(hotkey,
                             QKeySequence {QString::fromStdString(value)});
}

void HotkeyManager::HandleKeyPress(QKeyEvent* ev)
{
   logger_->trace("HandleKeyPress: {}, {}",
                  ev->keyCombination().toCombined(),
                  ev->isAutoRepeat());

   for (auto& hotkey : p->hotkeys_)
   {
      if (hotkey.second.count() == 1 &&
          hotkey.second[0] == ev->keyCombination())
      {
         Q_EMIT HotkeyPressed(hotkey.first, ev->isAutoRepeat());
      }
   }
}

void HotkeyManager::HandleKeyRelease(QKeyEvent* ev)
{
   logger_->trace("HandleKeyRelease: {}", ev->keyCombination().toCombined());

   for (auto& hotkey : p->hotkeys_)
   {
      if (hotkey.second.count() == 1 &&
          hotkey.second[0] == ev->keyCombination())
      {
         Q_EMIT HotkeyReleased(hotkey.first);
      }
   }
}

std::shared_ptr<HotkeyManager> HotkeyManager::Instance()
{
   static std::weak_ptr<HotkeyManager> hotkeyManagerReference_ {};
   static std::mutex                   instanceMutex_ {};

   std::unique_lock lock(instanceMutex_);

   std::shared_ptr<HotkeyManager> hotkeyManager =
      hotkeyManagerReference_.lock();

   if (hotkeyManager == nullptr)
   {
      hotkeyManager           = std::make_shared<HotkeyManager>();
      hotkeyManagerReference_ = hotkeyManager;
   }

   return hotkeyManager;
}

} // namespace manager
} // namespace qt
} // namespace scwx
