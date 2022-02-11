#pragma once

#include <scwx/wsr88d/nexrad_file.hpp>

namespace scwx
{
namespace wsr88d
{

class NexradFileFactory
{
private:
   explicit NexradFileFactory() = delete;
   ~NexradFileFactory()         = delete;

   NexradFileFactory(const NexradFileFactory&) = delete;
   NexradFileFactory& operator=(const NexradFileFactory&) = delete;

   NexradFileFactory(NexradFileFactory&&) noexcept = delete;
   NexradFileFactory& operator=(NexradFileFactory&&) noexcept = delete;

public:
   static std::shared_ptr<NexradFile> Create(const std::string& filename);
   static std::shared_ptr<NexradFile> Create(std::istream& is);
};

} // namespace wsr88d
} // namespace scwx
