#include <scwx/derivers/base_deriver.hpp>

#include <memory>
#include <unordered_map>
#include <utility>

namespace scwx::derivers
{

class BaseDeriver::Impl
{
public:
   explicit Impl() = default;
   std::shared_ptr<wsr88d::Ar2vFile> level2File_ = nullptr;
   std::unordered_map<std::string, std::shared_ptr<wsr88d::Level3File>>
      level3Files_ = {};
   bool changed_ = false;
};

BaseDeriver::BaseDeriver() : p {std::make_unique<Impl>()} {}
BaseDeriver::~BaseDeriver() = default;

void BaseDeriver::set_level_2_input_file(std::shared_ptr<wsr88d::Ar2vFile> file)
{
   if (needs_level_2_input())
   {
      p->level2File_ = std::move(file);
      p->changed_ = true;
   }
}

void BaseDeriver::set_level_3_input_file(
   const std::string& product, std::shared_ptr<wsr88d::Level3File> file)
{
   if (get_level_3_input_products().contains(product))
   {
      p->level3Files_.insert_or_assign(product, file);
      p->changed_ = true;
   }
}

std::shared_ptr<wsr88d::Ar2vFile> BaseDeriver::get_level_2_file()
{
   return p->level2File_;
}

std::shared_ptr<wsr88d::Level3File>
BaseDeriver::get_level_3_file(const std::string& product)
{
   auto file = p->level3Files_.find(product);
   if (file == p->level3Files_.end())
   {
      return nullptr;
   }
   return file->second;
}

bool BaseDeriver::get_changed()
{
   return p->changed_;
}

void BaseDeriver::set_changed(bool changed)
{
   p->changed_ = changed;
}

} // namespace scwx::deriver
