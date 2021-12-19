#version 330 core
layout (location = 0) in vec3 aVertex;
layout (location = 1) in vec4 aColor;

uniform mat4 uMVPMatrix;

out vec4 color;

void main()
{
   gl_Position = uMVPMatrix * vec4(aVertex, 1.0f);
   color       = aColor;
}
