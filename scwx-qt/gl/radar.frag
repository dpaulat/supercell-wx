#version 330 core

// Lower the default precision to medium
precision mediump float;

uniform sampler1D uTexture;
uniform uint uDataMomentOffset;
uniform float uDataMomentScale;
layout(std140) uniform LayerState
{
   float uOpacity;
};

uniform bool uCFPEnabled;

in float dataMoment;
in float cfpMoment;

layout (location = 0) out vec4 fragColor;

void main()
{
   float texCoord = (dataMoment - float(uDataMomentOffset)) / uDataMomentScale;

   if (uCFPEnabled && cfpMoment > 8u)
   {
      texCoord = texCoord - float(cfpMoment - 8u) / 2.0f;
   }

   fragColor = texture(uTexture, texCoord);
   fragColor.a *= uOpacity;
}
