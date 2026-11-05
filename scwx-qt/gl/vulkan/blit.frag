#version 450 core

layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0) uniform sampler2D uTexture;

void main()
{
   // 1:1 pixel copy of a premultiplied-alpha texture the size of the target.
   // Blended with One / OneMinusSrcAlpha by the pipeline.
   fragColor = texelFetch(uTexture, ivec2(gl_FragCoord.xy), 0);
}
