#version 450 core

layout(location = 0) in vec4 vColor;
layout(location = 0) out vec4 fragColor;

void main()
{
   // Straight alpha. The pipeline's SrcAlpha / OneMinusSrcAlpha blend
   // composites against whatever is already in the color target.
   fragColor = vec4(vColor.rgb, clamp(vColor.a, 0.0, 1.0));
}
