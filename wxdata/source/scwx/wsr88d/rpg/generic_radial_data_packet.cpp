#include <scwx/wsr88d/rpg/generic_radial_data_packet.hpp>

#include <string>

#include <boost/log/trivial.hpp>

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

static const std::string logPrefix_ =
   "[scwx::wsr88d::rpg::generic_radial_data_packet] ";

class GenericRadialDataPacketImpl
{
public:
   explicit GenericRadialDataPacketImpl() = default;
   ~GenericRadialDataPacketImpl()         = default;
};

GenericRadialDataPacket::GenericRadialDataPacket() :
    p(std::make_unique<GenericRadialDataPacketImpl>())
{
}
GenericRadialDataPacket::~GenericRadialDataPacket() = default;

GenericRadialDataPacket::GenericRadialDataPacket(
   GenericRadialDataPacket&&) noexcept                    = default;
GenericRadialDataPacket& GenericRadialDataPacket::operator=(
   GenericRadialDataPacket&&) noexcept = default;

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
