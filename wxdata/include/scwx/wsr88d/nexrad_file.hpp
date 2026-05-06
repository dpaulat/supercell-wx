#pragma once

#include <memory>
#include <string>

namespace scwx
{
namespace wsr88d
{

class NexradFileImpl;

class NexradFile
{
public:
   NexradFile(const NexradFile&)            = delete;
   NexradFile& operator=(const NexradFile&) = delete;

   virtual ~NexradFile();

   virtual bool LoadFile(const std::string& filename) = 0;
   virtual bool LoadData(std::istream& is)            = 0;

protected:
   explicit NexradFile();

   NexradFile(NexradFile&&) noexcept;
   NexradFile& operator=(NexradFile&&) noexcept;

private:
   std::unique_ptr<NexradFileImpl> p;
};

} // namespace wsr88d
} // namespace scwx
