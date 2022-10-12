#pragma once

#include <memory>
#include <string>

#include <QObject>

namespace scwx
{
namespace qt
{
namespace manager
{

class TextEventManager : public QObject
{
   Q_OBJECT

public:
   explicit TextEventManager();
   ~TextEventManager();

   void LoadFile(const std::string& filename);

   static TextEventManager& Instance();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace manager
} // namespace qt
} // namespace scwx
