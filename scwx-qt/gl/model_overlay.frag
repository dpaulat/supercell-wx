#version 330 core

uniform sampler2D uTexture;
uniform float uOpacity;

smooth in vec2 texCoord;
out vec4 fragColor;

void main()
{
   vec4 color = texture(uTexture, texCoord);
   fragColor = vec4(color.rgb, color.a * uOpacity);
}
