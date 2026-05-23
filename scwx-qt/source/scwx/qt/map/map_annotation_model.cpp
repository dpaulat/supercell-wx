#include <scwx/qt/map/map_annotation_model.hpp>

#include <algorithm>
#include <mutex>
#include <vector>

namespace scwx::qt::map
{

class MapAnnotationModel::Impl
{
public:
   mutable std::mutex               mutex_;
   std::vector<MapAnnotationObject> objects_;
   std::uint64_t                    nextId_ {1};
};

MapAnnotationModel::MapAnnotationModel() : p(std::make_unique<Impl>()) {}

MapAnnotationModel::~MapAnnotationModel() = default;

std::uint64_t MapAnnotationModel::Add(MapAnnotationObject object)
{
   const std::lock_guard lock {p->mutex_};
   object.id = p->nextId_++;
   p->objects_.push_back(std::move(object));
   return p->objects_.back().id;
}

void MapAnnotationModel::Remove(std::uint64_t id)
{
   const std::lock_guard lock {p->mutex_};
   std::erase_if(p->objects_,
                 [id](const MapAnnotationObject& o) { return o.id == id; });
}

void MapAnnotationModel::Clear()
{
   const std::lock_guard lock {p->mutex_};
   p->objects_.clear();
}

void MapAnnotationModel::Read(
   const std::function<void(const std::vector<MapAnnotationObject>&)>& fn) const
{
   const std::lock_guard lock {p->mutex_};
   fn(p->objects_);
}

void MapAnnotationModel::Write(
   const std::function<void(std::vector<MapAnnotationObject>&)>& fn)
{
   const std::lock_guard lock {p->mutex_};
   fn(p->objects_);
}

} // namespace scwx::qt::map
