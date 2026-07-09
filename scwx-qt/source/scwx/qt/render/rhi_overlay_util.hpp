#pragma once

#include <scwx/qt/render/rhi_overlay_phase.hpp>

#include <rhi/qrhi.h>

class QRhi;
class QRhiCommandBuffer;
class QRhiResourceUpdateBatch;

namespace scwx::qt::render
{

[[nodiscard]] inline QRhiResourceUpdateBatch*
AcquireOverlayBatch(QRhi*                    rhi,
                    QRhiResourceUpdateBatch* externalBatch,
                    RhiOverlayPhase          phase)
{
   if (phase == RhiOverlayPhase::Draw)
   {
      return nullptr;
   }
   if (externalBatch != nullptr)
   {
      return externalBatch;
   }
   return rhi->nextResourceUpdateBatch();
}

inline void SubmitOverlayBatch(QRhiCommandBuffer*       commandBuffer,
                               QRhiResourceUpdateBatch* batch,
                               QRhiResourceUpdateBatch* externalBatch,
                               RhiOverlayPhase          phase)
{
   if (batch == nullptr || phase == RhiOverlayPhase::Upload)
   {
      return;
   }
   if (externalBatch != nullptr)
   {
      return;
   }
   commandBuffer->resourceUpdate(batch);
}

[[nodiscard]] inline bool OverlayShouldUpload(RhiOverlayPhase phase)
{
   return phase != RhiOverlayPhase::Draw;
}

[[nodiscard]] inline bool OverlayShouldDraw(RhiOverlayPhase phase)
{
   return phase != RhiOverlayPhase::Upload;
}

} // namespace scwx::qt::render
