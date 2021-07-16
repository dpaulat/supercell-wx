#version 330 core

// Lower the default precision to medium
precision mediump float;

uniform sampler2D uTexture;

in vec2 texCoord;

void main()
{
   gl_FragColor = vec4(1.0f, 0.0f, 0.0f, 1.0f);
}
