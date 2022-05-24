#include <scwx/provider/level2_data_provider.hpp>

namespace scwx
{
namespace provider
{

static const std::string logPrefix_ = "scwx::provider::level2_data_provider";

class Level2DataProviderImpl
{
public:
   explicit Level2DataProviderImpl() {}

   ~Level2DataProviderImpl() {}
};

Level2DataProvider::Level2DataProvider() :
    p(std::make_unique<Level2DataProviderImpl>())
{
}
Level2DataProvider::~Level2DataProvider() = default;

Level2DataProvider::Level2DataProvider(Level2DataProvider&&) noexcept = default;
Level2DataProvider&
Level2DataProvider::operator=(Level2DataProvider&&) noexcept = default;

} // namespace provider
} // namespace scwx
