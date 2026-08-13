#version 450 core

layout(location = 0) in vec4 vColor;
layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 1) uniform sampler2D uBackdrop;

void main()
{
   float a = clamp(vColor.a, 0.0, 1.0);

   // Strokes (high alpha): replace into the current color target so they sit
   // on top of fills already drawn this pass. Mixing against the pre-pass
   // backdrop here erased borders (stroke lost under the fill wash).
   if (a >= 0.45)
   {
      fragColor = vec4(vColor.rgb, 1.0);
      return;
   }

   // Fills (low alpha): manual composite against the pre-pass backdrop copy
   // (SrcAlpha blend is unreliable on this preserve-contents pass).
   vec3 backdrop = texelFetch(uBackdrop, ivec2(gl_FragCoord.xy), 0).rgb;
   fragColor     = vec4(mix(backdrop, vColor.rgb, a), 1.0);
}
