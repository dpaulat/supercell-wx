#version 330 core

#define LATITUDE_MAX 85.051128779806604f
#define PI_OVER_4 0.785398163397448309615660825f
#define PI_OVER_360 0.00872664625997164788461845361111f
#define RAD2DEG 57.295779513082320876798156332941f

layout (location = 0) in vec2 aLatLong;
layout (location = 1) in vec2 aTexCoord;

uniform mat4 uMapMatrix;
uniform vec2 uOriginLatLong;

smooth out vec2 texCoord;

void main()
{
   vec2 latLng = vec2(clamp(aLatLong.x, -LATITUDE_MAX, LATITUDE_MAX),
                      aLatLong.y);
   vec2 deltaLatLng = latLng - uOriginLatLong;
   deltaLatLng.y = mod(deltaLatLng.y + 180.0f, 360.0f) - 180.0f;
   vec2 deltaScreen = vec2(
      deltaLatLng.y,
      RAD2DEG * log(tan(PI_OVER_4 + latLng.x * PI_OVER_360)) -
      RAD2DEG * log(tan(PI_OVER_4 + uOriginLatLong.x * PI_OVER_360)));
   gl_Position = uMapMatrix * vec4(deltaScreen, 0.0f, 1.0f);
   texCoord = aTexCoord;
}
