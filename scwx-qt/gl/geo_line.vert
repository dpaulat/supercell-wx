#version 330 core

#define LATITUDE_MAX  85.051128779806604f
#define PI_OVER_4     0.785398163397448309615660825f
#define PI_OVER_360   0.00872664625997164788461845361111f
#define RAD2DEG       57.295779513082320876798156332941f

layout (location = 0) in vec2 aLatLong;
layout (location = 1) in vec2 aXYOffset;
layout (location = 2) in vec3 aTexCoord;
layout (location = 3) in vec4 aModulate;

uniform mat4 uMVPMatrix;
uniform mat4 uMapMatrix;
uniform vec2 uOriginLatLong;

smooth out vec3 texCoord;
smooth out vec4 color;

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
   // Pass the texture coordinate and color modulate to the fragment shader
   texCoord = aTexCoord;
   color    = aModulate;

   vec2 p = latLngToDeltaScreenCoordinate(aLatLong);

   // Transform the position to screen coordinates
   gl_Position = uMapMatrix * vec4(p, 0.0f, 1.0f) -
                 uMVPMatrix * vec4(aXYOffset, 0.0f, 0.0f);
}
