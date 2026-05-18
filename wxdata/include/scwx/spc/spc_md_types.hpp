#pragma once

#include <scwx/common/geographic.hpp>

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>
#include <utility>

namespace scwx
{
namespace spc
{

struct MesoscaleDiscussion
{
   std::int32_t mdNumber_ {0};
   std::string  name_ {};
   std::string  description_ {}; // Full HTML
   std::string  kmlUrl_ {};      // URL to individual MD KMZ for polygon data
   std::vector<std::vector<std::pair<double, double>>>
                      rings_; // [ring][point] = (lat, lon)
   common::Coordinate centroid_ {};
};

struct MdData
{
   std::chrono::system_clock::time_point updateTime_ {};
   std::vector<MesoscaleDiscussion>      discussions_ {};
};

} // namespace spc
} // namespace scwx
