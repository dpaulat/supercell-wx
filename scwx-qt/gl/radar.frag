#version 330 core

// Lower the default precision to medium
precision mediump float;

uniform sampler2D uTexture;

flat in uint dataMoment;

layout (location = 0) out vec4 fragColor;

void main()
{
   if (dataMoment < 126u)
   {
      fragColor = vec4(0.0f, 0.5f, 0.0f, 0.9f);
   }
   else if (dataMoment < 156u)
   {
      fragColor = vec4(1.0f, 0.75f, 0.0f, 0.9f);
   }
   else
   {
      fragColor = vec4(1.0f, 0.0f, 0.0f, 0.9f);
   }
}
