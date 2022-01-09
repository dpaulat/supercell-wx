#include <scwx/wsr88d/rpg/point_feature_symbol_packet.hpp>

#include <istream>
#include <string>

#include <boost/log/trivial.hpp>

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

static const std::string logPrefix_ =
   "[scwx::wsr88d::rpg::point_feature_symbol_packet] ";

struct PointFeature
{
   int16_t  iPosition_;
   int16_t  jPosition_;
   uint16_t pointFeatureType_;
   uint16_t pointFeatureAttribute_;

   PointFeature() :
       iPosition_ {0},
       jPosition_ {0},
       pointFeatureType_ {0},
       pointFeatureAttribute_ {0}
   {
   }
};

class PointFeatureSymbolPacketImpl
{
public:
   explicit PointFeatureSymbolPacketImpl() : pointFeature_ {}, recordCount_ {0}
   {
   }
   ~PointFeatureSymbolPacketImpl() = default;

   std::vector<PointFeature> pointFeature_;
   size_t                    recordCount_;
};

PointFeatureSymbolPacket::PointFeatureSymbolPacket() :
    p(std::make_unique<PointFeatureSymbolPacketImpl>())
{
}
PointFeatureSymbolPacket::~PointFeatureSymbolPacket() = default;

PointFeatureSymbolPacket::PointFeatureSymbolPacket(
   PointFeatureSymbolPacket&&) noexcept                     = default;
PointFeatureSymbolPacket& PointFeatureSymbolPacket::operator=(
   PointFeatureSymbolPacket&&) noexcept = default;

size_t PointFeatureSymbolPacket::MinBlockLength() const
{
   return 8;
}

size_t PointFeatureSymbolPacket::MaxBlockLength() const
{
   return 32760;
}

int16_t PointFeatureSymbolPacket::i_position(size_t i) const
{
   return p->pointFeature_[i].iPosition_;
}

int16_t PointFeatureSymbolPacket::j_position(size_t i) const
{
   return p->pointFeature_[i].jPosition_;
}

uint16_t PointFeatureSymbolPacket::point_feature_type(size_t i) const
{
   return p->pointFeature_[i].pointFeatureType_;
}

uint16_t PointFeatureSymbolPacket::point_feature_attribute(size_t i) const
{
   return p->pointFeature_[i].pointFeatureAttribute_;
}

size_t PointFeatureSymbolPacket::RecordCount() const
{
   return p->recordCount_;
}

bool PointFeatureSymbolPacket::ParseData(std::istream& is)
{
   bool blockValid = true;

   if (packet_code() != 20)
   {
      BOOST_LOG_TRIVIAL(warning)
         << logPrefix_ << "Invalid packet code: " << packet_code();
      blockValid = false;
   }

   if (blockValid)
   {
      p->recordCount_ = length_of_block() / 8;
      p->pointFeature_.resize(p->recordCount_);

      for (size_t i = 0; i < p->recordCount_; i++)
      {
         PointFeature& f = p->pointFeature_[i];

         is.read(reinterpret_cast<char*>(&f.iPosition_), 2);
         is.read(reinterpret_cast<char*>(&f.jPosition_), 2);
         is.read(reinterpret_cast<char*>(&f.pointFeatureType_), 2);
         is.read(reinterpret_cast<char*>(&f.pointFeatureAttribute_), 2);

         f.iPosition_             = ntohs(f.iPosition_);
         f.jPosition_             = ntohs(f.jPosition_);
         f.pointFeatureType_      = ntohs(f.pointFeatureType_);
         f.pointFeatureAttribute_ = ntohs(f.pointFeatureAttribute_);
      }
   }

   return blockValid;
}

std::shared_ptr<PointFeatureSymbolPacket>
PointFeatureSymbolPacket::Create(std::istream& is)
{
   std::shared_ptr<PointFeatureSymbolPacket> packet =
      std::make_shared<PointFeatureSymbolPacket>();

   if (!packet->Parse(is))
   {
      packet.reset();
   }

   return packet;
}

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
