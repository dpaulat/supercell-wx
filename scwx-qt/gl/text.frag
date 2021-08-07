#version 330 core

in vec2 texCoords;
in vec4 textColor;

out vec4 color;

uniform sampler2D uTexture;

void main()
{
   float dist  = texture(uTexture, texCoords).r;
   float width = fwidth(dist);
   float alpha = smoothstep(0.5f - width, 0.5f + width, dist);

   color = vec4(textColor.rgb, textColor.a * alpha);
}
