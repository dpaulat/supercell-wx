#version 330 core

// Lower the default precision to medium
precision mediump float;

uniform sampler2DArray uTexture;

smooth in vec3 texCoord;
smooth in vec4 color;

layout (location = 0) out vec4 fragColor;

void main()
{
   fragColor = texture(uTexture, texCoord) * color;
}
