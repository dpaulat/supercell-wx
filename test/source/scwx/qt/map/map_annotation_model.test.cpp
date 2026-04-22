#include <scwx/qt/map/map_annotation_model.hpp>

#include <gtest/gtest.h>

namespace scwx::qt::map
{

TEST(MapAnnotationModelTest, AddRemoveAndClear)
{
   MapAnnotationModel model;

   MapAnnotationObject line {};
   line.payload = MapAnnotationPolyline {};

   MapAnnotationObject circle {};
   circle.payload = MapAnnotationCircle {};

   const std::uint64_t lineId   = model.Add(line);
   const std::uint64_t circleId = model.Add(circle);

   EXPECT_EQ(lineId, 1u);
   EXPECT_EQ(circleId, 2u);

   std::vector<std::uint64_t> ids;
   model.Read(
      [&](const std::vector<MapAnnotationObject>& objects)
      {
         ids.reserve(objects.size());
         for (const auto& object : objects)
         {
            ids.push_back(object.id);
         }
      });
   EXPECT_EQ(ids.size(), 2u);
   EXPECT_EQ(ids[0], lineId);
   EXPECT_EQ(ids[1], circleId);

   model.Remove(lineId);

   ids.clear();
   model.Read(
      [&](const std::vector<MapAnnotationObject>& objects)
      {
         ids.reserve(objects.size());
         for (const auto& object : objects)
         {
            ids.push_back(object.id);
         }
      });
   ASSERT_EQ(ids.size(), 1u);
   EXPECT_EQ(ids[0], circleId);

   model.Clear();
   model.Read([&](const std::vector<MapAnnotationObject>& objects)
              { EXPECT_TRUE(objects.empty()); });
}

TEST(MapAnnotationModelTest, WriteUpdatesStoredObject)
{
   MapAnnotationModel model;

   MapAnnotationObject    object {};
   MapAnnotationRectangle rectangle {};
   rectangle.fill = false;
   object.payload = rectangle;

   const std::uint64_t id = model.Add(object);

   model.Write(
      [id](std::vector<MapAnnotationObject>& objects)
      {
         for (auto& entry : objects)
         {
            if (entry.id != id)
            {
               continue;
            }

            auto* updated = std::get_if<MapAnnotationRectangle>(&entry.payload);
            ASSERT_NE(updated, nullptr);
            updated->fill = true;
         }
      });

   model.Read(
      [&](const std::vector<MapAnnotationObject>& objects)
      {
         ASSERT_EQ(objects.size(), 1u);
         const auto* stored =
            std::get_if<MapAnnotationRectangle>(&objects.front().payload);
         ASSERT_NE(stored, nullptr);
         EXPECT_TRUE(stored->fill);
      });
}

} // namespace scwx::qt::map
