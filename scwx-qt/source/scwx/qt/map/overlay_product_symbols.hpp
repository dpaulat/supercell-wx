#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace scwx::qt::map
{

inline constexpr std::string_view kNstProduct {"NST"};
inline constexpr std::string_view kNhiProduct {"NHI"};
inline constexpr std::string_view kNmeProduct {"NME"};
inline constexpr std::string_view kNtvProduct {"NTV"};
inline constexpr std::string_view kNmdProduct {"NMD"};

inline constexpr std::size_t kOverlayIconWidth  = 32;
inline constexpr std::size_t kOverlayIconHeight = 32;

// Hail icon sheet indices
inline constexpr std::size_t kHailIconSmall  = 0;
inline constexpr std::size_t kHailIconMedium = 1;
inline constexpr std::size_t kHailIconLarge  = 2;
inline constexpr std::size_t kHailIconSevere = 3;

// Mesocyclone icon sheet indices
inline constexpr std::size_t kMesoIconShear           = 0;
inline constexpr std::size_t kMesoIconCirculation     = 1;
inline constexpr std::size_t kMesoIconElevatedStrong  = 2;
inline constexpr std::size_t kMesoIconLowLevelStrong  = 3;

// TVS icon sheet indices
inline constexpr std::size_t kTvsIconTvs  = 0;
inline constexpr std::size_t kTvsIconEtvs = 1;

const std::vector<std::string>& OverlayProductNames();
bool IsOverlayProduct(const std::string& product);
bool IsOverlayProductCode(std::int16_t productCode);

bool        HailSymbolVisible(std::int16_t probabilityOfHail);
std::size_t HailIconIndex(std::int16_t  probabilityOfHail,
                          std::int16_t  probabilityOfSevereHail,
                          std::uint16_t maxHailSize);
std::string FormatHailSize(std::uint16_t maxHailSize);
std::string FormatProbability(std::int16_t probability);
std::string HailHoverText(const std::string& stormId,
                          std::int16_t       probabilityOfHail,
                          std::int16_t       probabilityOfSevereHail,
                          std::uint16_t      maxHailSize);

std::size_t MesocycloneIconIndexFromFeatureType(std::uint16_t featureType);
std::size_t MesocycloneIconIndexFromPacketCode(std::uint16_t packetCode);
bool        IsMesocycloneFeatureType(std::uint16_t featureType);
std::string PointFeatureTypeName(std::uint16_t featureType);
std::string MesocycloneHoverText(const std::string& label,
                                 std::uint16_t      featureType,
                                 std::uint16_t      radiusQuarterKm);
std::string LegacyMesocycloneHoverText(const std::string& label,
                                       std::uint16_t      packetCode,
                                       std::int16_t       radiusQuarterKm);

std::size_t TvsIconIndexFromFeatureType(std::uint16_t featureType);
std::size_t TvsIconIndexFromPacketCode(std::uint16_t packetCode);
bool        IsTvsFeatureType(std::uint16_t featureType);
std::string TvsTypeName(std::uint16_t packetCode);
std::string TvsHoverText(const std::string& label, const std::string& typeName);

std::string TrimLabel(std::string text);

} // namespace scwx::qt::map
