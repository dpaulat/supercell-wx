#version 450 core

layout(location = 0) in vec3 vTexCoord;
layout(location = 1) in vec4 vColor;
layout(location = 2) in flat float vDisplayed;

layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 1) uniform sampler2DArray uTexture;

void main()
{
   if (vDisplayed == 0.0)
   {
      discard;
   }

   fragColor = texture(uTexture, vTexCoord) * vColor;
}
