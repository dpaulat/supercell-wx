#version 330 core

// Lower the default precision to medium
precision mediump float;

uniform sampler1D uTexture;

in float texCoord;

layout (location = 0) out vec4 fragColor;

void main()
{
   fragColor = texture(uTexture, texCoord);
}
