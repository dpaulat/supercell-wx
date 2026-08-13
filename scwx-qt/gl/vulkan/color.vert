#version 450 core

layout(location = 0) in vec3 aVertex;
layout(location = 1) in vec4 aColor;

layout(location = 0) out vec4 vColor;

layout(set = 0, binding = 0) uniform UniformBlock {
    mat4 uMVPMatrix;
};

void main()
{
   gl_Position = uMVPMatrix * vec4(aVertex, 1.0);
   vColor      = aColor;
}
