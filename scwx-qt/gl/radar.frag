#version 330 core

// Lower the default precision to medium
precision mediump float;

uniform sampler1D uTexture;
uniform uint uDataMomentOffset;
uniform float uDataMomentScale;

uniform bool uCFPEnabled;

flat in uint dataMoment;
flat in uint cfpMoment;

layout (location = 0) out vec4 fragColor;

void main()
{
   float texCoord = float(dataMoment - uDataMomentOffset) / uDataMomentScale;

   if (uCFPEnabled && cfpMoment > 8u)
   {
      texCoord = texCoord - float(cfpMoment - 8u) / 2.0f;
   }

   fragColor = texture(uTexture, texCoord);
}
