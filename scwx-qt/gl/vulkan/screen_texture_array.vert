#version 450 core

#define DEG2RAD 0.0174532925199432957692369055556

layout(location = 0) in vec2 aVertex;
layout(location = 1) in vec2 aXYOffset;
layout(location = 2) in vec3 aTexCoord;
layout(location = 3) in vec4 aModulate;
layout(location = 4) in float aAngleDeg;
layout(location = 5) in float aDisplayed;

layout(location = 0) out vec3 vTexCoord;
layout(location = 1) out vec4 vColor;
layout(location = 2) out flat float vDisplayed;

layout(set = 0, binding = 0) uniform UniformBlock {
    mat4 uMVPMatrix;
};

void main()
{
   vTexCoord  = aTexCoord;
   vColor     = aModulate;
   vDisplayed = aDisplayed;

   float angle  = aAngleDeg * DEG2RAD;
   mat2  rotate = mat2(cos(angle), -sin(angle), sin(angle), cos(angle));
   vec2  pos    = aVertex + rotate * aXYOffset;

   gl_Position = uMVPMatrix * vec4(pos, 0.0, 1.0);
}
