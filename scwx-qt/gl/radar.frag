#version 330 core

// Lower the default precision to medium
precision mediump float;

uniform sampler1D uTexture;

flat in uint dataMoment;

layout (location = 0) out vec4 fragColor;

void main()
{
   float texCoord = float(dataMoment - 2u) / 253.0f; // TODO: Scale properly

   fragColor = texture(uTexture, texCoord);
}
