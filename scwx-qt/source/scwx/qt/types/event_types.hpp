#pragma once

#include <functional>

class QEvent;

namespace scwx
{
namespace qt
{
namespace types
{

struct EventHandler
{
   std::function<void(QEvent*)> event_ {};
};

} // namespace types
} // namespace qt
} // namespace scwx
