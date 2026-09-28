#version 330 core

// POC: rectangular-grid counterpart to radar.vert. Instead of one vertex
// per data point (radar's polar sweep), this draws a single quad covering
// the grid's lat/lon bounding box and lets grib.frag sample the data
// texture per-fragment -- the data volume here (MRMS CONUS is 24.5M
// cells) makes per-vertex attributes impractical.

#define LATITUDE_MAX  85.051128779806604f
#define PI_OVER_4     0.785398163397448309615660825f
#define PI_OVER_360   0.00872664625997164788461845361111f
#define RAD2DEG       57.295779513082320876798156332941f

layout (location = 0) in vec2 aLatLong;
layout (location = 1) in vec2 aTexCoord;

uniform mat4 uMVPMatrix;
uniform vec2 uOriginLatLong;

out vec2 texCoord;

vec2 latLngToDeltaScreenCoordinate(in vec2 latLng)
{
   latLng.x = clamp(latLng.x, -LATITUDE_MAX, LATITUDE_MAX);

   vec2 deltaLatLng = latLng - uOriginLatLong;

   vec2 deltaScreen = vec2(
      deltaLatLng.y,
      RAD2DEG * log(tan(PI_OVER_4 + (uOriginLatLong.x + deltaLatLng.x) * PI_OVER_360)) -
      RAD2DEG * log(tan(PI_OVER_4 + uOriginLatLong.x * PI_OVER_360))
   );

   return deltaScreen;
}

void main()
{
   texCoord = aTexCoord;

   vec2 p = latLngToDeltaScreenCoordinate(aLatLong);

   gl_Position = uMVPMatrix * vec4(p, 0.0f, 1.0f);
}
