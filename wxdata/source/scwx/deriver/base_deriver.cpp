#include <scwx/deriver/base_deriver.hpp>
#include <scwx/util/logger.hpp>

#include <memory>
#include <utility>
#include <shared_mutex>

namespace scwx::deriver
{
static const std::string logPrefix_ = "scwx::deriver::base_deriver";
static const auto        logger_    = util::Logger::Create(logPrefix_);

class BaseDeriver::Impl
{
public:
   explicit Impl(BaseDeriver* self) : self_ {self} {};

   BaseDeriver* const self_;

   std::unordered_map<
      wsr88d::rda::DataBlockType,
      std::unordered_map<float, std::shared_ptr<wsr88d::rda::ElevationScan>>>
                     level2Data_ = {};
   std::shared_mutex level2DataMutex_ {};
   std::unordered_map<std::string, std::shared_ptr<wsr88d::rpg::Level3Message>>
                     level3Data_ {};
   std::shared_mutex level3DataMutex_ {};
};

BaseDeriver::BaseDeriver() : p {std::make_unique<Impl>(this)} {}
BaseDeriver::~BaseDeriver() = default;

void BaseDeriver::SetLevel2Input(
   wsr88d::rda::DataBlockType                  dataBlockType,
   float                                       elevation,
   std::shared_ptr<wsr88d::rda::ElevationScan> data)
{
   std::unique_lock lock {p->level2DataMutex_};
   auto             ofDataBlockType = p->level2Data_.find(dataBlockType);
   if (ofDataBlockType == p->level2Data_.end())
   {
      std::unordered_map<float, std::shared_ptr<wsr88d::rda::ElevationScan>>
         innerMap = {{elevation, data}};
      p->level2Data_.emplace(dataBlockType, innerMap);
   }
   else
   {
      ofDataBlockType->second.insert_or_assign(elevation, data);
   }
}

void BaseDeriver::SetLevel3Input(
   const std::string& product, std::shared_ptr<wsr88d::rpg::Level3Message> data)
{
   std::unique_lock lock {p->level3DataMutex_};
   p->level3Data_.insert_or_assign(product, data);
}

std::shared_ptr<wsr88d::rda::ElevationScan>
BaseDeriver::GetLevel2Input(wsr88d::rda::DataBlockType dataBlockType,
                            float                      elevation)
{
   std::shared_lock lock {p->level2DataMutex_};
   auto             ofBlockType = p->level2Data_.find(dataBlockType);
   if (ofBlockType == p->level2Data_.end())
   {
      return nullptr;
   }
   auto dataIt = ofBlockType->second.find(elevation);
   if (dataIt == ofBlockType->second.end())
   {
      return nullptr;
   }

   return dataIt->second;
}

std::shared_ptr<wsr88d::rpg::Level3Message>
BaseDeriver::GetLevel3Input(const std::string& product)
{
   std::shared_lock lock {p->level3DataMutex_};
   auto             file = p->level3Data_.find(product);
   if (file == p->level3Data_.end())
   {
      return nullptr;
   }
   return file->second;
}

} // namespace scwx::deriver
