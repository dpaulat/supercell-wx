#version 450 core

layout(location = 0) in vec2 aVertex;
layout(location = 1) in float aTexCoord;

layout(location = 0) out float vTexCoord;

layout(set = 0, binding = 0) uniform UniformBlock {
    mat4 uMVPMatrix;
};

void main()
{
   gl_Position = uMVPMatrix * vec4(aVertex, 0.0, 1.0);
   vTexCoord   = aTexCoord;
}
