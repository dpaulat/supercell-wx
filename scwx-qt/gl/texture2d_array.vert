#version 330 core

#define DEG2RAD 0.0174532925199432957692369055556f

layout (location = 0) in vec2  aVertex;
layout (location = 1) in vec2  aXYOffset;
layout (location = 2) in vec3  aTexCoord;
layout (location = 3) in vec4  aModulate;
layout (location = 4) in float aAngleDeg;

uniform mat4 uMVPMatrix;

smooth out vec3 texCoord;
smooth out vec4 color;

void main()
{
   // Rotate clockwise
   float angle  = aAngleDeg * DEG2RAD;
   mat2  rotate = mat2(cos(angle), -sin(angle),
                       sin(angle), cos(angle));

   gl_Position = uMVPMatrix * vec4(aVertex + rotate * aXYOffset, 0.0f, 1.0f);
   texCoord    = aTexCoord;
   color       = aModulate;
}
