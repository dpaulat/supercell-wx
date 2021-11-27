#pragma once

#include <memory>

#include <QObject>
#include <QMapboxGL>

namespace scwx
{
namespace qt
{
namespace map
{

class GenericLayerImpl;

class GenericLayer : public QObject
{
   Q_OBJECT

public:
   explicit GenericLayer();
   virtual ~GenericLayer();

   virtual void Initialize() = 0;
   virtual void Render(const QMapbox::CustomLayerRenderParameters&) = 0;
   virtual void Deinitialize() = 0;

private:
   std::unique_ptr<GenericLayerImpl> p;
};

} // namespace map
} // namespace qt
} // namespace scwx
