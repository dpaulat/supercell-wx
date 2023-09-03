#version 330 core

layout (location = 0) in vec2 aScreenCoord;
layout (location = 1) in vec2 aXYOffset;
layout (location = 2) in vec4 aColor;
layout (location = 3) in int  aThreshold;

uniform mat4 uMVPMatrix;
uniform mat4 uMapMatrix;
uniform vec2 uMapScreenCoord;

out VertexData
{
   int  threshold;
   vec3 texCoord;
   vec4 color;
} vsOut;

smooth out vec4 color;

void main()
{
   // Pass the threshold and color to the geometry shader
   vsOut.threshold = aThreshold;

   // Pass the color to the geometry and fragment shaders
   vsOut.color = aColor;
   color       = aColor;

   vec2 p = aScreenCoord - uMapScreenCoord;

   // Transform the position to screen coordinates
   gl_Position = uMapMatrix * vec4(p, 0.0f, 1.0f) +
                 uMVPMatrix * vec4(aXYOffset, 0.0f, 0.0f);
}
