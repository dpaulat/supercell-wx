#pragma once

#include <scwx/spc/spc_types.hpp>

#include <string>

namespace scwx::spc
{

OutlookData ParseSpcGeoJson(OutlookDay         day,
                            OutlookProduct     product,
                            const std::string& jsonBody);

} // namespace scwx::spc
