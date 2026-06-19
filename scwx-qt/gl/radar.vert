#version 330 core

#define LATITUDE_MAX  85.051128779806604f
#define PI_OVER_4     0.785398163397448309615660825f
#define PI_OVER_360   0.00872664625997164788461845361111f
#define RAD2DEG       57.295779513082320876798156332941f

layout (location = 0) in vec2 aLatLong;
layout (location = 1) in uint aDataMoment;
layout (location = 2) in uint aCfpMoment;

uniform mat4 uMVPMatrix;
uniform vec2 uOriginLatLong;

out float dataMoment;
out float cfpMoment;

vec2 latLngToDeltaScreenCoordinate(in vec2 latLng)
{
   latLng.x = clamp(latLng.x, -LATITUDE_MAX, LATITUDE_MAX);

   // Convert to smaller, relative coordinates
   vec2 deltaLatLng = latLng - uOriginLatLong;

   // Apply projection to the delta
   vec2 deltaScreen = vec2(
      deltaLatLng.y,
      RAD2DEG * log(tan(PI_OVER_4 + (uOriginLatLong.x + deltaLatLng.x) * PI_OVER_360)) -
      RAD2DEG * log(tan(PI_OVER_4 + uOriginLatLong.x * PI_OVER_360))
   );

   return deltaScreen;
}

void main()
{
   // Pass the coded data moment to the fragment shader
   dataMoment = aDataMoment;
   cfpMoment  = aCfpMoment;

   vec2 p = latLngToDeltaScreenCoordinate(aLatLong);

   // Transform the position to screen coordinates
   gl_Position = uMVPMatrix * vec4(p, 0.0f, 1.0f);
}
