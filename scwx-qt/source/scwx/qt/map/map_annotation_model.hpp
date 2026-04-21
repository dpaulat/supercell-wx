#pragma once

#include <scwx/qt/map/map_annotation_types.hpp>

#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <vector>

namespace scwx::qt::map
{

class MapAnnotationModel
{
public:
   MapAnnotationModel()  = default;
   ~MapAnnotationModel() = default;

   MapAnnotationModel(const MapAnnotationModel&)            = delete;
   MapAnnotationModel& operator=(const MapAnnotationModel&) = delete;

   [[nodiscard]] std::uint64_t Add(MapAnnotationObject object);
   void                        Remove(std::uint64_t id);
   void                        Clear();

   void
   Read(std::function<void(const std::vector<MapAnnotationObject>&)> fn) const;
   void Write(std::function<void(std::vector<MapAnnotationObject>&)> fn);

private:
   mutable std::mutex               mutex_;
   std::vector<MapAnnotationObject> objects_;
   std::uint64_t                    nextId_ {1};
};

} // namespace scwx::qt::map
