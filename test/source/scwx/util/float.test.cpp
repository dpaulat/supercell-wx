#include <scwx/util/float.hpp>

#include <gtest/gtest.h>

namespace scwx
{
namespace util
{

TEST(FloatTest, Decode32Positive1)
{
   uint16_t msw = 0x3f80;
   uint16_t lsw = 0x0000;

   float x = DecodeFloat32(msw, lsw);

   EXPECT_FLOAT_EQ(x, 1.0f);
}

TEST(FloatTest, Decode32Negative1)
{
   uint16_t msw = 0xbf80;
   uint16_t lsw = 0x0000;

   float x = DecodeFloat32(msw, lsw);

   EXPECT_FLOAT_EQ(x, -1.0f);
}

TEST(FloatTest, Decode32Positive12345678)
{
   uint16_t msw = 0x4b3c;
   uint16_t lsw = 0x614e;

   float x = DecodeFloat32(msw, lsw);

   EXPECT_FLOAT_EQ(x, 12345678.0f);
}

TEST(FloatTest, Decode16h0x5bb4)
{
   uint16_t hex = 0x5bb4;

   float x = DecodeFloat16(hex);

   EXPECT_FLOAT_EQ(x, 123.25f);
}

} // namespace util
} // namespace scwx
