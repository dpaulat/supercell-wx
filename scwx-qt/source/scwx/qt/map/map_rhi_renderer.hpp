#pragma once

#include <QMapLibre/Map>

class QRhi;
class QRhiCommandBuffer;
class QRhiTexture;
class QWindow;

namespace scwx::qt::map
{

class MapRhiRenderer
{
public:
   void InitializeMapRenderer(QRhi* rhi, QWindow* window, QMapLibre::Map* map);
   void RenderMap(QRhiTexture* colorTexture, QMapLibre::Map* map);
   void ReleaseMapRenderer(QMapLibre::Map* map);

   static void CopyColorTexture(QRhi*              rhi,
                                QRhiCommandBuffer* commandBuffer,
                                QRhiTexture*       destination,
                                QRhiTexture*       source);

   [[nodiscard]] bool IsInitialized() const;

private:
   bool initialized_ {false};
};

} // namespace scwx::qt::map
