#version 330 core

smooth in vec4  color;
smooth in float vArcLen;
smooth in vec2  vMercatorHint;
smooth in float vDashPeriod;
smooth in float vDashDuty;

uniform int uHatchMode;
layout(std140) uniform LayerState
{
   float uOpacity;
};

layout(location = 0) out vec4 fragColor;

void main()
{
   if (uHatchMode == 0 && vDashPeriod > 1.0e-3)
   {
      float x = mod(vArcLen, vDashPeriod);
      if (x > vDashPeriod * vDashDuty)
      {
         discard;
      }
   }

   if (uHatchMode != 0)
   {
      float scale = 6.28318530718 * 0.00002;
      float h     = sin(vMercatorHint.x * scale) * sin(vMercatorHint.y * scale);
      if (h < 0.0)
      {
         discard;
      }
   }

   fragColor = color;
   fragColor.a *= uOpacity;
}
