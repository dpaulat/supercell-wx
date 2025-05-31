#include <scwx/deriver/base_deriver.hpp>

#include <memory>
#include <unordered_map>
#include <utility>

namespace scwx::deriver
{

class BaseDeriver::Impl
{
public:
   explicit Impl()                               = default;
   std::shared_ptr<wsr88d::Ar2vFile> level2File_ = nullptr;
   std::unordered_map<std::string, std::shared_ptr<wsr88d::Level3File>>
        level3Files_ = {};
   bool changed_     = false;
};

BaseDeriver::BaseDeriver() : p {std::make_unique<Impl>()} {}
BaseDeriver::~BaseDeriver() = default;

void BaseDeriver::SetLevel2InputFile(std::shared_ptr<wsr88d::Ar2vFile> file)
{
   if (NeedsLevel2Input())
   {
      p->level2File_ = std::move(file);
      p->changed_    = true;
   }
}

void BaseDeriver::SetLevel3InputFile(const std::string& product,
                                     std::shared_ptr<wsr88d::Level3File> file)
{
   if (GetLevel3InputProducts().contains(product))
   {
      p->level3Files_.insert_or_assign(product, file);
      p->changed_ = true;
   }
}

std::shared_ptr<wsr88d::Ar2vFile> BaseDeriver::GetLevel2File()
{
   return p->level2File_;
}

std::shared_ptr<wsr88d::Level3File>
BaseDeriver::GetLevel3File(const std::string& product)
{
   auto file = p->level3Files_.find(product);
   if (file == p->level3Files_.end())
   {
      return nullptr;
   }
   return file->second;
}

bool BaseDeriver::GetChanged()
{
   return p->changed_;
}

void BaseDeriver::SetChanged(bool changed)
{
   p->changed_ = changed;
}

} // namespace scwx::deriver
