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
protected:
   explicit NexradFile();

   NexradFile(const NexradFile&) = delete;
   NexradFile& operator=(const NexradFile&) = delete;

   NexradFile(NexradFile&&) noexcept;
   NexradFile& operator=(NexradFile&&) noexcept;

public:
   virtual ~NexradFile();

   virtual bool LoadFile(const std::string& filename) = 0;
   virtual bool LoadData(std::istream& is)            = 0;

private:
   std::unique_ptr<NexradFileImpl> p;
};

} // namespace wsr88d
} // namespace scwx
