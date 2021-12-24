#include <scwx/wsr88d/rpg/vector2d.hpp>

#include <array>
#include <istream>
#include <set>
#include <string>

#include <boost/log/trivial.hpp>

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

static const std::string logPrefix_ = "[scwx::wsr88d::rpg::vector2d] ";

class Vector2DImpl
{
public:
   explicit Vector2DImpl() : startI_ {}, startJ_ {}, endI_ {}, endJ_ {} {};
   ~Vector2DImpl() = default;

   int16_t startI_;
   int16_t startJ_;
   int16_t endI_;
   int16_t endJ_;
};

Vector2D::Vector2D() : p(std::make_unique<Vector2DImpl>()) {}
Vector2D::~Vector2D() = default;

Vector2D::Vector2D(Vector2D&&) noexcept = default;
Vector2D& Vector2D::operator=(Vector2D&&) noexcept = default;

int16_t Vector2D::start_i() const
{
   return p->startI_;
}

int16_t Vector2D::start_j() const
{
   return p->startJ_;
}

int16_t Vector2D::end_i() const
{
   return p->endI_;
}

int16_t Vector2D::end_j() const
{
   return p->endJ_;
}

size_t Vector2D::data_size() const
{
   return SIZE;
}

bool Vector2D::Parse(std::istream& is)
{
   bool blockValid = true;

   is.read(reinterpret_cast<char*>(&p->startI_), 2);
   is.read(reinterpret_cast<char*>(&p->startJ_), 2);
   is.read(reinterpret_cast<char*>(&p->endI_), 2);
   is.read(reinterpret_cast<char*>(&p->endJ_), 2);

   p->startI_ = ntohs(p->startI_);
   p->startJ_ = ntohs(p->startJ_);
   p->endI_   = ntohs(p->endI_);
   p->endJ_   = ntohs(p->endJ_);

   if (is.eof())
   {
      BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "Reached end of file";
      blockValid = false;
   }

   return blockValid;
}

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
