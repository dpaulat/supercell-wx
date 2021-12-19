#version 330 core
in vec4 color;

layout (location = 0) out vec4 fragColor;

void main()
{
   fragColor = color;
}
