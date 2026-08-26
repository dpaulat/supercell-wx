#version 330 core

// Lower the default precision to medium
precision mediump float;

uniform sampler2DArray uTexture;
layout(std140) uniform LayerState
{
   float uOpacity;
};

smooth in vec3 texCoord;
smooth in vec4 color;

layout (location = 0) out vec4 fragColor;

void main()
{
   fragColor = texture(uTexture, texCoord) * color;
   fragColor.a *= uOpacity;
}
