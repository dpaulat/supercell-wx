#pragma once

#include <scwx/qt/map/map_annotation_types.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace scwx::qt::map
{

class MapAnnotationModel
{
public:
   MapAnnotationModel();
   ~MapAnnotationModel();

   MapAnnotationModel(const MapAnnotationModel&)            = delete;
   MapAnnotationModel& operator=(const MapAnnotationModel&) = delete;
   MapAnnotationModel(MapAnnotationModel&&)                 = delete;
   MapAnnotationModel& operator=(MapAnnotationModel&&)      = delete;

   [[nodiscard]] std::uint64_t Add(MapAnnotationObject object);
   void                        Remove(std::uint64_t id);
   void                        Clear();

   /** Holds `mutex_` for the whole call; do not call back into this model. */
   void Read(const std::function<void(const std::vector<MapAnnotationObject>&)>&
                fn) const;
   /** Holds `mutex_` for the whole call; do not call back into this model. */
   void Write(const std::function<void(std::vector<MapAnnotationObject>&)>& fn);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::map
