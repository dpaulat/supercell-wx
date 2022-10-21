#pragma once

#include <scwx/qt/map/map_context.hpp>

#include <memory>

#include <QObject>
#include <QMapLibreGL/QMapLibreGL>

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
   explicit GenericLayer(std::shared_ptr<MapContext> context);
   virtual ~GenericLayer();

   virtual void Initialize()                                            = 0;
   virtual void Render(const QMapLibreGL::CustomLayerRenderParameters&) = 0;
   virtual void Deinitialize()                                          = 0;

protected:
   std::shared_ptr<MapContext> context() const;

private:
   std::unique_ptr<GenericLayerImpl> p;
};

} // namespace map
} // namespace qt
} // namespace scwx
