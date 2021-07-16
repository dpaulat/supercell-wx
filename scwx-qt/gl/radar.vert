#version 330 core

layout (location = 0) in vec2 aPosition;
layout (location = 1) in vec2 aTexCoord;

uniform mat4 uMVPMatrix;

out vec2 texCoord;

void main()
{
   // Pass the texture coordinate to the fragment shader
   texCoord = aTexCoord;

   // Transform the position to screen coordinates
   gl_Position = uMVPMatrix * vec4(aPosition, 0.0f, 1.0f);
}
