#version 450 core

layout(location = 0) in flat int vThreshold;
layout(location = 1) in flat int vStartTime;
layout(location = 2) in flat int vEndTime;
layout(location = 3) in flat int vDisplayed;
layout(location = 4) in vec4 vColor;
layout(location = 5) in flat vec4 vHighlightColor;
layout(location = 6) in flat vec4 vBorderColor;
layout(location = 7) in flat vec3 vStrokeHalf;
layout(location = 8) in float vOffsetY;

layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0) uniform UniformBlock {
    mat4 uMVPMatrix;
    mat4 uMapMatrix;
    vec2 uOriginLatLong;
    float uMapDistance;
    int uSelectedTime;
};

bool IsVisible()
{
   if (vDisplayed == 0)
   {
      return false;
   }

   if (!(vThreshold == 0 || uMapDistance == 0.0 ||
         (vThreshold < 0 && float(-vThreshold) <= uMapDistance) ||
         float(vThreshold) >= uMapDistance || vThreshold >= 999))
   {
      return false;
   }

   if (vStartTime != 0 &&
       (vStartTime > uSelectedTime || uSelectedTime >= vEndTime))
   {
      return false;
   }

   return true;
}

void main()
{
   if (!IsVisible())
   {
      discard;
   }

   if (vStrokeHalf.z <= 0.0)
   {
      fragColor = vColor;
      return;
   }

   const float d = abs(vOffsetY);
   if (d > vStrokeHalf.z)
   {
      discard;
   }
   else if (d > vStrokeHalf.y)
   {
      fragColor = vBorderColor;
   }
   else if (d > vStrokeHalf.x)
   {
      fragColor = vHighlightColor;
   }
   else
   {
      fragColor = vColor;
   }
}
