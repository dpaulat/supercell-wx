#version 330 core

layout (location = 0) in vec3 aVertex;
layout (location = 1) in vec2 aTexCoords;
layout (location = 2) in vec4 aColor;

out vec2 texCoords;
out vec4 textColor;

uniform mat4 projection;

void main()
{
   gl_Position = projection * vec4(aVertex, 1.0f);
   texCoords   = aTexCoords;
   textColor   = aColor;
}
