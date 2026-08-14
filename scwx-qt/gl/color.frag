#version 330 core
smooth in vec4 color;

uniform float uOpacity;

layout (location = 0) out vec4 fragColor;

void main()
{
   fragColor = color;
   fragColor.a *= uOpacity;
}
