#pragma once

#include <scwx/qt/types/hotkey_types.hpp>

#include <memory>

#include <QObject>

class QKeyEvent;

namespace scwx
{
namespace qt
{
namespace manager
{

class HotkeyManager : public QObject
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(HotkeyManager)

public:
   explicit HotkeyManager();
   ~HotkeyManager();

   void HandleKeyPress(QKeyEvent* event);
   void HandleKeyRelease(QKeyEvent* event);

   static std::shared_ptr<HotkeyManager> Instance();

signals:
   void HotkeyPressed(scwx::qt::types::Hotkey hotkey, bool isAutoRepeat);
   void HotkeyReleased(scwx::qt::types::Hotkey hotkey);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace manager
} // namespace qt
} // namespace scwx
