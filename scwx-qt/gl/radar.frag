#version 330 core

// Lower the default precision to medium
precision mediump float;

uniform sampler1D uTexture;
uniform uint uDataMomentOffset;
uniform float uDataMomentScale;

flat in uint dataMoment;

layout (location = 0) out vec4 fragColor;

void main()
{
   float texCoord = float(dataMoment - uDataMomentOffset) / uDataMomentScale;

   fragColor = texture(uTexture, texCoord);
}
