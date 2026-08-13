#version 450 core

layout(location = 0) in flat uint vDataMoment;
layout(location = 1) in flat uint vCfpMoment;

layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0) uniform UniformBlock {
    mat4 uMVPMatrix;
    mat4 uMapMatrix;
    vec2 uOriginLatLong;
    uint uDataMomentOffset;
    float uDataMomentScale;
    int uCFPEnabled;
};

layout(set = 0, binding = 1) uniform sampler2D uTexture;

void main()
{
   float texCoord =
      (float(vDataMoment) - float(uDataMomentOffset)) / uDataMomentScale;

   if (uCFPEnabled != 0 && vCfpMoment > 8u)
   {
      texCoord = texCoord - float(vCfpMoment - 8u) / 2.0;
   }

   fragColor = texture(uTexture, vec2(texCoord, 0.5));
}
