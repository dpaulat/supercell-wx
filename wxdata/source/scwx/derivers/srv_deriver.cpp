
#include <scwx/derivers/srv_deriver.hpp>

namespace scwx::deriver
{

static const std::unordered_set<std::string> kLevel3InputProducts_ = {"SRM",
                                                                      "SDV"};

class SrvDeriver::Impl
{
public:
   explicit Impl() = default;

   std::unordered_map<std::string, std::shared_ptr<wsr88d::NexradFile>>
      inputFiles_ = {};
   bool changed_ {false};
   std::shared_ptr<wsr88d::NexradFile> outputFile_ {nullptr};
};

SrvDeriver::SrvDeriver() : p {std::make_unique<Impl>()} {}

const std::unordered_set<std::string>& SrvDeriver::get_level_3_input_products()
{
   return kLevel3InputProducts_;
}

bool SrvDeriver::needs_level_2_input()
{
   return false;
}

void SrvDeriver::set_level_3_input_file(
   const std::string& product, std::shared_ptr<wsr88d::NexradFile> file)
{
   if (kLevel3InputProducts_.contains(product))
   {
      p->inputFiles_.insert_or_assign(product, file);
      p->changed_ = true;
   }
}

void SrvDeriver::set_level_2_input_file(std::shared_ptr<wsr88d::NexradFile>) {}

std::shared_ptr<wsr88d::NexradFile> SrvDeriver::get_output_file()
{
   if (!p->changed_)
   {
      return p->outputFile_;
   }

   const auto& srmIt = p->inputFiles_.find("SRM");
   const auto& sdvIt = p->inputFiles_.find("SDV");
   if (srmIt == p->inputFiles_.cend() || sdvIt == p->inputFiles_.cend())
   {
      return p->outputFile_;
   }

   const std::shared_ptr<wsr88d::NexradFile>& srmFile = srmIt->second;
   const std::shared_ptr<wsr88d::NexradFile>& sdvFile = sdvIt->second;

   return p->outputFile_;
}

} // namespace scwx::deriver
