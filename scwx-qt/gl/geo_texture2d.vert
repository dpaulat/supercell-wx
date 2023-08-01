#version 330 core

#define DEGREES_MAX   360.0f
#define LATITUDE_MAX  85.051128779806604f
#define LONGITUDE_MAX 180.0f
#define PI            3.1415926535897932384626433f
#define RAD2DEG       57.295779513082320876798156332941f

layout (location = 0) in vec2  aLatLong;
layout (location = 1) in vec2  aXYOffset;
layout (location = 2) in vec2  aTexCoord;
layout (location = 3) in vec4  aModulate;
layout (location = 4) in float aAngle;

uniform mat4 uMVPMatrix;
uniform mat4 uMapMatrix;
uniform vec2 uMapScreenCoord;

smooth out vec2 texCoord;
flat   out vec4 modulate;

vec2 latLngToScreenCoordinate(in vec2 latLng)
{
   vec2 p;
   latLng.x = clamp(latLng.x, -LATITUDE_MAX, LATITUDE_MAX);
   p.xy     = vec2(LONGITUDE_MAX + latLng.y,
                   -(LONGITUDE_MAX - RAD2DEG * log(tan(PI / 4 + latLng.x * PI / DEGREES_MAX))));
   return p;
}

void main()
{
   // Pass the texture coordinate and color modulate to the fragment shader
   texCoord = aTexCoord;
   modulate = aModulate;

   vec2 p = latLngToScreenCoordinate(aLatLong) - uMapScreenCoord;

   // Transform the position to screen coordinates
   gl_Position = uMapMatrix * vec4(p, 0.0f, 1.0f) -
                 uMVPMatrix * vec4(aXYOffset, 0.0f, 0.0f);
}
