#include <scwx/wsr88d/nexrad_file.hpp>

namespace scwx
{
namespace wsr88d
{

class NexradFileImpl
{
public:
   explicit NexradFileImpl() {}
   ~NexradFileImpl() = default;
};

NexradFile::NexradFile() : p(std::make_unique<NexradFileImpl>()) {}
NexradFile::~NexradFile() = default;

NexradFile::NexradFile(NexradFile&&) noexcept = default;
NexradFile& NexradFile::operator=(NexradFile&&) noexcept = default;

} // namespace wsr88d
} // namespace scwx
