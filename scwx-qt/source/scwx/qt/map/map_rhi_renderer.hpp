#pragma once

#include <QMapLibre/Map>

class QRhi;
class QRhiTexture;
class QWindow;

namespace scwx::qt::map
{

class MapRhiRenderer
{
public:
   void InitializeMapRenderer(QRhi*           rhi,
                              QWindow*        window,
                              QMapLibre::Map* map);
   void RenderMap(QRhiTexture* colorTexture, QMapLibre::Map* map);
   void ReleaseMapRenderer(QMapLibre::Map* map);

   [[nodiscard]] bool IsInitialized() const;

private:
   bool initialized_ {false};
};

} // namespace scwx::qt::map
