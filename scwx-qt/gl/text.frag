#version 330 core
in vec2 texCoords;
out vec4 color;

uniform sampler2D text;
uniform vec4 textColor;

void main()
{
   vec4 sampled = vec4(1.0f, 1.0f, 1.0f, texture(text, texCoords).r);
   color        = textColor * sampled;
}
