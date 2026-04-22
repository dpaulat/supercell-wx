#include <scwx/qt/map/map_annotation_model.hpp>

#include <algorithm>

namespace scwx::qt::map
{

std::uint64_t MapAnnotationModel::Add(MapAnnotationObject object)
{
   const std::lock_guard lock {mutex_};
   object.id = nextId_++;
   objects_.push_back(std::move(object));
   return objects_.back().id;
}

void MapAnnotationModel::Remove(std::uint64_t id)
{
   const std::lock_guard lock {mutex_};
   std::erase_if(objects_,
                 [id](const MapAnnotationObject& o) { return o.id == id; });
}

void MapAnnotationModel::Clear()
{
   const std::lock_guard lock {mutex_};
   objects_.clear();
}

void MapAnnotationModel::Read(
   const std::function<void(const std::vector<MapAnnotationObject>&)>& fn) const
{
   const std::lock_guard lock {mutex_};
   fn(objects_);
}

void MapAnnotationModel::Write(
   const std::function<void(std::vector<MapAnnotationObject>&)>& fn)
{
   const std::lock_guard lock {mutex_};
   fn(objects_);
}

} // namespace scwx::qt::map
