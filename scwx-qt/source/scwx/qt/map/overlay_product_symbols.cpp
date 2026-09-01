#include <scwx/qt/map/overlay_product_symbols.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <unordered_map>
#include <unordered_set>

#include <fmt/format.h>

namespace scwx::qt::map
{

namespace
{

constexpr std::int16_t kBeyondRangeProbability = -999;

const std::array<std::string, 5> kOverlayProducts_ {std::string {kNstProduct},
                                                    std::string {kNhiProduct},
                                                    std::string {kNmdProduct},
                                                    std::string {kNtvProduct},
                                                    std::string {kNmeProduct}};

const std::unordered_set<std::int16_t> kOverlayProductCodes_ {
   58, 59, 60, 61, 141};

const std::unordered_map<std::uint16_t, std::string> kPointFeatureTypeNames_ {
   {1, "Mesocyclone (Extrapolated)"},
   {2, "3-D Correlated Shear (Extrapolated)"},
   {3, "Mesocyclone"},
   {4, "3-D Correlated Shear"},
   {5, "TVS (Extrapolated)"},
   {6, "ETVS (Extrapolated)"},
   {7, "TVS"},
   {8, "ETVS"},
   {9, "MDA Circulation (Rank >= 5, Low-level)"},
   {10, "MDA Circulation (Rank >= 5, Elevated)"},
   {11, "MDA Circulation (Rank < 5)"}};

} // namespace

const std::vector<std::string>& OverlayProductNames()
{
   static const std::vector<std::string> names {kOverlayProducts_.begin(),
                                                kOverlayProducts_.end()};
   return names;
}

bool IsOverlayProduct(const std::string& product)
{
   return std::find(kOverlayProducts_.begin(),
                    kOverlayProducts_.end(),
                    product) != kOverlayProducts_.end();
}

bool IsOverlayProductCode(std::int16_t productCode)
{
   return kOverlayProductCodes_.contains(productCode);
}

bool HailSymbolVisible(std::int16_t probabilityOfHail)
{
   return probabilityOfHail >= 0 &&
          probabilityOfHail != kBeyondRangeProbability;
}

std::size_t HailIconIndex(std::int16_t /* probabilityOfHail */,
                          std::int16_t  probabilityOfSevereHail,
                          std::uint16_t maxHailSize)
{
   if (probabilityOfSevereHail > 0)
   {
      return maxHailSize >= 2 ? kHailIconSevere : kHailIconLarge;
   }

   if (maxHailSize >= 2)
   {
      return kHailIconLarge;
   }
   if (maxHailSize >= 1)
   {
      return kHailIconMedium;
   }

   return kHailIconSmall;
}

std::string FormatHailSize(std::uint16_t maxHailSize)
{
   if (maxHailSize == 0)
   {
      return "<0.25 in";
   }

   return fmt::format("{} in", maxHailSize);
}

std::string FormatProbability(std::int16_t probability)
{
   if (probability < 0)
   {
      return "N/A";
   }

   return fmt::format("{}%", probability);
}

std::string HailHoverText(const std::string& stormId,
                          std::int16_t       probabilityOfHail,
                          std::int16_t       probabilityOfSevereHail,
                          std::uint16_t      maxHailSize)
{
   std::string hoverText = "Hail Index";

   if (!stormId.empty())
   {
      hoverText += fmt::format("\nStorm ID: {}", stormId);
   }

   hoverText += fmt::format("\nProbability of Hail: {}",
                            FormatProbability(probabilityOfHail));
   hoverText += fmt::format("\nProbability of Severe Hail: {}",
                            FormatProbability(probabilityOfSevereHail));
   hoverText += fmt::format("\nMax Hail Size: {}", FormatHailSize(maxHailSize));

   return hoverText;
}

bool IsMesocycloneFeatureType(std::uint16_t featureType)
{
   return featureType == 1 || featureType == 2 || featureType == 3 ||
          featureType == 4 || featureType == 9 || featureType == 10 ||
          featureType == 11;
}

std::size_t MesocycloneIconIndexFromFeatureType(std::uint16_t featureType)
{
   switch (featureType)
   {
   case 9:
      return kMesoIconLowLevelStrong;
   case 10:
      return kMesoIconElevatedStrong;
   case 1:
   case 3:
      return kMesoIconCirculation;
   case 2:
   case 4:
   case 11:
   default:
      return kMesoIconShear;
   }
}

std::size_t MesocycloneIconIndexFromPacketCode(std::uint16_t packetCode)
{
   // Packet 3 = mesocyclone, packet 11 = 3-D correlated shear
   return packetCode == 3 ? kMesoIconCirculation : kMesoIconShear;
}

std::string PointFeatureTypeName(std::uint16_t featureType)
{
   auto it = kPointFeatureTypeNames_.find(featureType);
   if (it != kPointFeatureTypeNames_.cend())
   {
      return it->second;
   }

   return fmt::format("Point Feature {}", featureType);
}

std::string MesocycloneHoverText(const std::string& label,
                                 std::uint16_t      featureType,
                                 std::uint16_t      radiusQuarterKm)
{
   std::string hoverText = "Mesocyclone Detection";

   if (!label.empty())
   {
      hoverText += fmt::format("\nCirculation ID: {}", label);
   }

   hoverText += fmt::format("\nType: {}", PointFeatureTypeName(featureType));
   hoverText += fmt::format("\nRadius: {:.2f} km", radiusQuarterKm * 0.25);
   return hoverText;
}

std::string LegacyMesocycloneHoverText(const std::string& label,
                                       std::uint16_t      packetCode,
                                       std::int16_t       radiusQuarterKm)
{
   std::string hoverText = "Mesocyclone";

   if (!label.empty())
   {
      hoverText += fmt::format("\nStorm ID: {}", label);
   }

   hoverText += fmt::format(
      "\nType: {}", packetCode == 11 ? "3-D Correlated Shear" : "Mesocyclone");
   hoverText += fmt::format("\nRadius: {:.2f} km", radiusQuarterKm * 0.25);
   return hoverText;
}

bool IsTvsFeatureType(std::uint16_t featureType)
{
   return featureType >= 5 && featureType <= 8;
}

std::size_t TvsIconIndexFromFeatureType(std::uint16_t featureType)
{
   return (featureType == 6 || featureType == 8) ? kTvsIconEtvs : kTvsIconTvs;
}

std::size_t TvsIconIndexFromPacketCode(std::uint16_t packetCode)
{
   // Packet 26 = ETVS, packet 12 = TVS
   return packetCode == 26 ? kTvsIconEtvs : kTvsIconTvs;
}

std::string TvsTypeName(std::uint16_t packetCode)
{
   return packetCode == 26 ? "ETVS" : "TVS";
}

std::string TvsHoverText(const std::string& label, const std::string& typeName)
{
   std::string hoverText =
      fmt::format("Tornadic Vortex Signature\nType: {}", typeName);

   if (!label.empty())
   {
      hoverText += fmt::format("\nStorm ID: {}", label);
   }

   return hoverText;
}

std::string TrimLabel(std::string text)
{
   auto notSpace = [](unsigned char ch)
   {
      return !std::isspace(ch);
   };

   text.erase(text.begin(), std::find_if(text.begin(), text.end(), notSpace));
   text.erase(std::find_if(text.rbegin(), text.rend(), notSpace).base(),
              text.end());

   // Packet 8 text can contain embedded NULs
   auto nul = std::find(text.begin(), text.end(), '\0');
   text.erase(nul, text.end());

   return text;
}

} // namespace scwx::qt::map
