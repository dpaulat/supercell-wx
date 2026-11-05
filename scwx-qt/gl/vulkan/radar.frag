#version 450 core

layout(location = 0) in float vDataMoment;
layout(location = 1) in float vCfpMoment;

layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0) uniform UniformBlock {
    mat4 uMVPMatrix;
    mat4 uMapMatrix;
    vec2 uOriginLatLong;
    uint uDataMomentOffset;
    float uDataMomentScale;
    int uCFPEnabled;
    float uOpacity;
};

layout(set = 0, binding = 1) uniform sampler2D uTexture;

void main()
{
   float texCoord =
      (vDataMoment - float(uDataMomentOffset)) / uDataMomentScale;

   if (uCFPEnabled != 0 && vCfpMoment > 8.0)
   {
      texCoord = texCoord - (vCfpMoment - 8.0) / 2.0;
   }

   fragColor = texture(uTexture, vec2(texCoord, 0.5));
   fragColor.a *= uOpacity;
}
