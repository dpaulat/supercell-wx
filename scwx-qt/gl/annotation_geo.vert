#version 330 core

#define LATITUDE_MAX 85.051128779806604f
#define PI_OVER_4    0.785398163397448309615660825f
#define PI_OVER_360  0.00872664625997164788461845361111f
#define RAD2DEG      57.295779513082320876798156332941f

layout(location = 0) in vec2 aLatLong;
layout(location = 1) in vec4 aColor;
layout(location = 2) in float aArcLen;
layout(location = 3) in vec2 aMercatorHint;
layout(location = 4) in vec2 aDash;

uniform mat4 uMapMatrix;
uniform vec2 uOriginLatLong;

smooth out vec4  color;
smooth out float vArcLen;
smooth out vec2  vMercatorHint;
smooth out float vDashPeriod;
smooth out float vDashDuty;

vec2 latLngToDeltaScreenCoordinate(in vec2 latLng)
{
   latLng.x = clamp(latLng.x, -LATITUDE_MAX, LATITUDE_MAX);

   vec2 deltaLatLng = latLng - uOriginLatLong;

   vec2 deltaScreen =
      vec2(deltaLatLng.y,
           RAD2DEG * log(tan(PI_OVER_4 + (uOriginLatLong.x + deltaLatLng.x) *
                                            PI_OVER_360)) -
              RAD2DEG * log(tan(PI_OVER_4 + uOriginLatLong.x * PI_OVER_360)));

   return deltaScreen;
}

void main()
{
   vec2 p        = latLngToDeltaScreenCoordinate(aLatLong);
   gl_Position   = uMapMatrix * vec4(p, 0.0f, 1.0f);
   color         = aColor;
   vArcLen       = aArcLen;
   vMercatorHint = aMercatorHint;
   vDashPeriod   = aDash.x;
   vDashDuty     = aDash.y;
}
