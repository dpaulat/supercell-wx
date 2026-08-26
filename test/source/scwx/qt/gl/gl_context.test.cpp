#include <scwx/qt/gl/gl_context.hpp>

#include <cstddef>

#include <gtest/gtest.h>

namespace scwx::qt::gl
{

TEST(LayerStateBlock, Std140Layout)
{
   EXPECT_EQ(kLayerStateBindingPoint, 16u);
   EXPECT_STREQ(kLayerStateBlockName, "LayerState");
   EXPECT_EQ(sizeof(LayerStateBlock), 16u);
   EXPECT_EQ(offsetof(LayerStateBlock, opacity), 0u);
   EXPECT_EQ(alignof(LayerStateBlock), 16u);

   const LayerStateBlock block {};
   EXPECT_FLOAT_EQ(block.opacity, 1.0f);
}

} // namespace scwx::qt::gl
