#version 450 core

#define LATITUDE_MAX  85.051128779806604
#define PI_OVER_4     0.785398163397448309615660825
#define PI_OVER_360   0.00872664625997164788461845361111
#define RAD2DEG       57.295779513082320876798156332941

layout(location = 0) in vec2 aLatLong;
layout(location = 1) in uint aDataMoment;
layout(location = 2) in uint aCfpMoment;

layout(location = 0) out flat uint vDataMoment;
layout(location = 1) out flat uint vCfpMoment;

layout(set = 0, binding = 0) uniform UniformBlock {
    mat4 uMVPMatrix;
    mat4 uMapMatrix;
    vec2 uOriginLatLong;
    uint uDataMomentOffset;
    float uDataMomentScale;
    int uCFPEnabled;
};

vec2 latLngToDeltaScreenCoordinate(vec2 latLng)
{
   latLng.x = clamp(latLng.x, -LATITUDE_MAX, LATITUDE_MAX);
   vec2 deltaLatLng = latLng - uOriginLatLong;
   return vec2(
      deltaLatLng.y,
      RAD2DEG * log(tan(PI_OVER_4 + (uOriginLatLong.x + deltaLatLng.x) * PI_OVER_360)) -
      RAD2DEG * log(tan(PI_OVER_4 + uOriginLatLong.x * PI_OVER_360)));
}

void main()
{
   vDataMoment = aDataMoment;
   vCfpMoment  = aCfpMoment;
   vec2 p      = latLngToDeltaScreenCoordinate(aLatLong);
   gl_Position = uMapMatrix * vec4(p, 0.0, 1.0);
}
