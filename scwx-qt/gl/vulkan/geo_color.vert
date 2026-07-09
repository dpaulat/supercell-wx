#version 450 core

#define LATITUDE_MAX  85.051128779806604
#define PI_OVER_4     0.785398163397448309615660825
#define PI_OVER_360   0.00872664625997164788461845361111
#define RAD2DEG       57.295779513082320876798156332941

layout(location = 0) in vec2 aLatLong;
layout(location = 1) in vec2 aXYOffset;
layout(location = 2) in vec4 aModulate;
// GeoLines/placefile lines: unrotated perpendicular offset (not degrees).
layout(location = 3) in float aAngleDeg;
layout(location = 4) in vec4 aHighlightColor;
layout(location = 5) in vec4 aBorderColor;
layout(location = 6) in vec3 aStrokeHalf;
layout(location = 7) in int aThreshold;
layout(location = 8) in int aStartTime;
layout(location = 9) in int aEndTime;
layout(location = 10) in int aDisplayed;

layout(location = 0) out flat int vThreshold;
layout(location = 1) out flat int vStartTime;
layout(location = 2) out flat int vEndTime;
layout(location = 3) out flat int vDisplayed;
layout(location = 4) out vec4 vColor;
layout(location = 5) out flat vec4 vHighlightColor;
layout(location = 6) out flat vec4 vBorderColor;
layout(location = 7) out flat vec3 vStrokeHalf;
layout(location = 8) out float vOffsetY;

layout(set = 0, binding = 0) uniform UniformBlock {
    mat4 uMVPMatrix;
    mat4 uMapMatrix;
    vec2 uOriginLatLong;
    float uMapDistance;
    int uSelectedTime;
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
   vThreshold        = aThreshold;
   vStartTime        = aStartTime;
   vEndTime          = aEndTime;
   vDisplayed        = aDisplayed;
   vColor            = aModulate;
   vHighlightColor   = aHighlightColor;
   vBorderColor      = aBorderColor;
   vStrokeHalf = aStrokeHalf;
   // Prefer aAngleDeg (CPU packs unrotated perpendicular there). Also accept
   // legacy buffers that still store degrees in aAngleDeg: if |aAngleDeg| looks
   // like a pixel offset (<= stroke outer half), use it; else fall back.
   vOffsetY = aAngleDeg;

   vec2 p = latLngToDeltaScreenCoordinate(aLatLong);

   // aXYOffset is CPU-rotated into screen pixel space already.
   gl_Position = uMapMatrix * vec4(p, 0.0, 1.0) +
                 uMVPMatrix * vec4(aXYOffset, 0.0, 0.0);
}
