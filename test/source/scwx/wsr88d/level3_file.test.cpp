#include <scwx/wsr88d/level3_file.hpp>

#include <gtest/gtest.h>

namespace scwx
{
namespace wsr88d
{

class ValidFileTest :
    public testing::TestWithParam<std::pair<int16_t, std::string>>
{
};

TEST_P(ValidFileTest, ValidFile)
{
   Level3File file;

   auto param = GetParam();

   const std::string filename {param.second};

   bool fileValid = file.LoadFile(std::string(SCWX_TEST_DATA_DIR) +
                                  "/nexrad/level3/" + filename);

   EXPECT_EQ(fileValid, true);
}

INSTANTIATE_TEST_SUITE_P(
   Level3File,
   ValidFileTest,
   testing::Values(
      std::pair<int16_t, std::string> {2, "KLSX_NXUS63_GSMLSX_202112110238"},
      std::pair<int16_t, std::string> {30, "KLSX_SDUS63_NSWLSX_202112110135"},
      std::pair<int16_t, std::string> {32, "KLSX_SDUS53_DHRLSX_202112110215"},
      std::pair<int16_t, std::string> {37, "KLSX_SDUS53_NCRLSX_202112110215"},
      std::pair<int16_t, std::string> {38, "KLSX_SDUS63_NCZLSX_202112110130"},
      std::pair<int16_t, std::string> {41, "KLSX_SDUS73_NETLSX_202112110152"},
      std::pair<int16_t, std::string> {48, "KLSX_SDUS33_NVWLSX_202112110119"},
      std::pair<int16_t, std::string> {56, "KLSX_SDUS23_N2SLSX_202112110109"},
      std::pair<int16_t, std::string> {57, "KLSX_SDUS53_NVLLSX_202112110130"},
      std::pair<int16_t, std::string> {58, "KLSX_SDUS33_NSTLSX_202112110215"},
      std::pair<int16_t, std::string> {59, "KLSX_SDUS63_NHILSX_202112110152"},
      std::pair<int16_t, std::string> {61, "KLSX_SDUS63_NTVLSX_202112110152"},
      std::pair<int16_t, std::string> {62, "KLSX_SDUS63_NSSLSX_202112110140"},
      std::pair<int16_t, std::string> {66, "KLSX_SDUS63_NMLLSX_202112110119"},
      std::pair<int16_t, std::string> {67, "KLSX_SDUS63_NLALSX_202112110119"},
      std::pair<int16_t, std::string> {74, "KLSX_SDUS43_RCMLSX_202112110244"},
      std::pair<int16_t, std::string> {78, "KLSX_SDUS33_N1PLSX_202112110103"},
      std::pair<int16_t, std::string> {79, "KLSX_SDUS63_N3PLSX_202112110209"},
      std::pair<int16_t, std::string> {80, "KLSX_SDUS53_NTPLSX_202112110114"},
      std::pair<int16_t, std::string> {81, "KLSX_SDUS53_DPALSX_202112110238"},
      std::pair<int16_t, std::string> {82, "KLSX_SDUS63_SPDLSX_202112110114"},
      std::pair<int16_t, std::string> {90, "KLSX_SDUS63_NHLLSX_202112110135"},
      std::pair<int16_t, std::string> {94, "KLSX_SDUS23_N2QLSX_202112110250"},
      std::pair<int16_t, std::string> {99, "KLSX_SDUS23_N1ULSX_202112110200"},
      std::pair<int16_t, std::string> {134, "KLSX_SDUS53_DVLLSX_202112110152"},
      std::pair<int16_t, std::string> {135, "KLSX_SDUS73_EETLSX_202112110215"},
      std::pair<int16_t, std::string> {138, "KLSX_SDUS53_DSPLSX_202112110109"},
      std::pair<int16_t, std::string> {141, "KLSX_SDUS33_NMDLSX_202112110152"},
      std::pair<int16_t, std::string> {152, "KLSX_SDUS43_RSLLSX_202112110256"},
      std::pair<int16_t, std::string> {159, "KLSX_SDUS83_N0XLSX_202112110212"},
      std::pair<int16_t, std::string> {161, "KLSX_SDUS83_N0CLSX_202112110106"},
      std::pair<int16_t, std::string> {163, "KLSX_SDUS83_N0KLSX_202112110229"},
      std::pair<int16_t, std::string> {165, "KLSX_SDUS83_N0HLSX_202112110212"},
      std::pair<int16_t, std::string> {166, "KLSX_SDUS83_N1MLSX_202112110200"},
      std::pair<int16_t, std::string> {169, "KLSX_SDUS83_OHALSX_202112110109"},
      std::pair<int16_t, std::string> {170, "KLSX_SDUS83_DAALSX_202112110135"},
      std::pair<int16_t, std::string> {172, "KLSX_SDUS83_DTALSX_202112110209"},
      std::pair<int16_t, std::string> {173, "KLSX_SDUS83_DU3LSX_202112110209"},
      std::pair<int16_t, std::string> {174, "KLSX_SDUS83_DODLSX_202112110244"},
      std::pair<int16_t, std::string> {175, "KLSX_SDUS83_DSDLSX_202112110135"},
      std::pair<int16_t, std::string> {176, "KLSX_SDUS83_DPRLSX_202112110140"},
      std::pair<int16_t, std::string> {177, "KLSX_SDUS83_HHCLSX_202112110140"},
      std::pair<int16_t, std::string> {99,
                                       "Level3_LSX_N1U_20211228_0446.nids"}));

} // namespace wsr88d
} // namespace scwx
