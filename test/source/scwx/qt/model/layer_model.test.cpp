#include <scwx/qt/model/layer_model.hpp>
#include <scwx/qt/types/layer_types.hpp>

#include <gtest/gtest.h>

namespace scwx::qt::model
{

class LayerModelOpacityTest : public ::testing::Test
{
protected:
   void SetUp() override
   {
      model_ = LayerModel::Instance();
      model_->ResetLayers();
   }

   std::shared_ptr<LayerModel> model_ {};
};

int FindRow(const std::shared_ptr<LayerModel>& model, types::LayerType type)
{
   for (int row = 0; row < model->rowCount(); ++row)
   {
      const QModelIndex typeIndex =
         model->index(row, static_cast<int>(LayerModel::Column::Type));
      if (types::GetLayerType(
             typeIndex.data(Qt::DisplayRole).toString().toStdString()) == type)
      {
         return row;
      }
   }
   return -1;
}

TEST_F(LayerModelOpacityTest, RadarOpacityIsEditable)
{
   const int radarRow = FindRow(model_, types::LayerType::Radar);
   ASSERT_GE(radarRow, 0);

   const QModelIndex opacityIndex =
      model_->index(radarRow, static_cast<int>(LayerModel::Column::Opacity));

   EXPECT_TRUE(model_->flags(opacityIndex) & Qt::ItemIsEditable);
   EXPECT_EQ(opacityIndex.data(Qt::DisplayRole).toString(), "100%");
   EXPECT_EQ(opacityIndex.data(Qt::EditRole).toInt(), 100);

   EXPECT_TRUE(model_->setData(opacityIndex, 40, Qt::ItemDataRole::EditRole));
   EXPECT_EQ(opacityIndex.data(Qt::EditRole).toInt(), 40);
   EXPECT_EQ(opacityIndex.data(Qt::DisplayRole).toString(), "40%");

   const types::LayerInfo info =
      model_->GetLayerInfo(types::LayerType::Radar, std::monostate {});
   EXPECT_FLOAT_EQ(info.opacity_, 0.4f);
}

TEST_F(LayerModelOpacityTest, MapStyleOpacityIsNotEditable)
{
   const int mapRow = FindRow(model_, types::LayerType::Map);
   ASSERT_GE(mapRow, 0);

   const QModelIndex opacityIndex =
      model_->index(mapRow, static_cast<int>(LayerModel::Column::Opacity));

   EXPECT_FALSE(model_->flags(opacityIndex) & Qt::ItemIsEditable);
   EXPECT_EQ(opacityIndex.data(Qt::DisplayRole).toString(), "Opaque");
   EXPECT_FALSE(model_->setData(opacityIndex, 25, Qt::ItemDataRole::EditRole));
}

TEST_F(LayerModelOpacityTest, SetLayerOpacityIgnoresMapLayers)
{
   EXPECT_FALSE(model_->SetLayerOpacity(
      types::LayerType::Map, types::MapLayer::MapUnderlay, 0.2f));

   EXPECT_TRUE(model_->SetLayerOpacity(
      types::LayerType::Radar, std::monostate {}, 0.55f));
   const types::LayerInfo info =
      model_->GetLayerInfo(types::LayerType::Radar, std::monostate {});
   EXPECT_FLOAT_EQ(info.opacity_, 0.55f);
}

TEST_F(LayerModelOpacityTest, ResetRestoresDefaultOpacity)
{
   ASSERT_TRUE(model_->SetLayerOpacity(
      types::LayerType::Radar, std::monostate {}, 0.3f));
   model_->ResetLayers();

   const types::LayerInfo info =
      model_->GetLayerInfo(types::LayerType::Radar, std::monostate {});
   EXPECT_FLOAT_EQ(info.opacity_, 1.0f);
}

} // namespace scwx::qt::model
