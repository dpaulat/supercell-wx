#pragma once

#include <units/angle.h>
#include <units/velocity.h>

namespace scwx
{
namespace common
{

struct StormMotionVector
{
   units::angle::degrees<float>  direction_ {};
   units::velocity::knots<float> speed_ {};
};

} // namespace common
} // namespace scwx
