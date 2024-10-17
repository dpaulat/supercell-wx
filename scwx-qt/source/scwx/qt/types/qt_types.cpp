#include <scwx/qt/types/qt_types.hpp>
#include <scwx/util/enum.hpp>

#include <boost/algorithm/string.hpp>

namespace scwx
{
namespace qt
{
namespace types
{

static const std::unordered_map<UiStyle, std::string> qtStyleName_ {
   {UiStyle::Default, "Default"},
   {UiStyle::Fusion, "Fusion"},
   {UiStyle::FusionLight, "Fusion"},
   {UiStyle::FusionDark, "Fusion"},
   {UiStyle::Unknown, "?"}};

static const std::unordered_map<UiStyle, std::string> uiStyleName_ {
   {UiStyle::Default, "Default"},
   {UiStyle::Fusion, "Fusion"},
   {UiStyle::FusionLight, "Fusion Light"},
   {UiStyle::FusionDark, "Fusion Dark"},
   {UiStyle::Unknown, "?"}};

static const std::unordered_map<UiStyle, Qt::ColorScheme> qtColorSchemeMap_ {
   {UiStyle::Default, Qt::ColorScheme::Unknown},
   {UiStyle::Fusion, Qt::ColorScheme::Unknown},
   {UiStyle::FusionLight, Qt::ColorScheme::Light},
   {UiStyle::FusionDark, Qt::ColorScheme::Dark},
   {UiStyle::Unknown, Qt::ColorScheme::Unknown}};

SCWX_GET_ENUM(UiStyle, GetUiStyle, uiStyleName_)

Qt::ColorScheme GetQtColorScheme(UiStyle uiStyle)
{
   return qtColorSchemeMap_.at(uiStyle);
}

std::string GetQtStyleName(UiStyle uiStyle)
{
   return qtStyleName_.at(uiStyle);
}

std::string GetUiStyleName(UiStyle uiStyle)
{
   return uiStyleName_.at(uiStyle);
}

} // namespace types
} // namespace qt
} // namespace scwx
