#version 330 core

// Lower the default precision to medium
precision mediump float;

uniform sampler2D uTexture;

flat in vec2 texCoord;
flat in vec4 modulate;

layout (location = 0) out vec4 fragColor;

void main()
{
   fragColor = texture(uTexture, texCoord) * modulate;
}
