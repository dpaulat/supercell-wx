#include <scwx/util/float.hpp>

#include <gtest/gtest.h>

namespace scwx
{
namespace util
{

class Float16Test :
    public testing ::TestWithParam<std ::pair<std::uint16_t, float>>
{
};

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

TEST_P(Float16Test, DecodeFloat16)
{
   auto param = GetParam();

   std::uint16_t hex = param.first;

   float x = DecodeFloat16(hex);

   EXPECT_FLOAT_EQ(x, param.second);
}

INSTANTIATE_TEST_SUITE_P(
   FloatTest,
   Float16Test,
   testing::Values(std::pair<std::uint16_t, float> {0x4400, 2.0f},
                   std::pair<std::uint16_t, float> {0x59ab, 90.6875f},
                   std::pair<std::uint16_t, float> {0x593e, 83.875f},
                   std::pair<std::uint16_t, float> {0x54dc, 38.875f},
                   std::pair<std::uint16_t, float> {0xc82a, -4.1640625f},
                   std::pair<std::uint16_t, float> {0x5bb4, 123.25f}));

} // namespace util
} // namespace scwx
