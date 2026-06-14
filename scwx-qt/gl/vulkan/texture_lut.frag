#version 450 core

layout(location = 0) in float vTexCoord;
layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 1) uniform sampler2D uTexture;

void main()
{
   fragColor = texture(uTexture, vec2(vTexCoord, 0.5));
}
