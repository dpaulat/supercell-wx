#version 330 core

#define DEG2RAD 0.0174532925199432957692369055556f

layout (location = 0) in vec2  aVertex;
layout (location = 1) in vec2  aXYOffset;
layout (location = 2) in vec3  aTexCoord;
layout (location = 3) in vec4  aModulate;
layout (location = 4) in float aAngleDeg;
layout (location = 5) in int   aDisplayed;

uniform mat4 uMVPMatrix;

out VertexData
{
   int   threshold;
   vec3  texCoord;
   vec4  color;
   ivec2 timeRange;
   bool  displayed;
} vsOut;

smooth out vec3 texCoord;
smooth out vec4 color;

void main()
{
   // Always set threshold and time range to zero
   vsOut.threshold = 0;
   vsOut.timeRange = ivec2(0, 0);

   // Pass displayed to the geometry shader
   vsOut.displayed = (aDisplayed != 0);

   // Pass the texture coordinate and color modulate to the geometry and
   // fragment shaders
   vsOut.texCoord = aTexCoord;
   vsOut.color    = aModulate;
   texCoord       = aTexCoord;
   color          = aModulate;

   // Rotate clockwise
   float angle  = aAngleDeg * DEG2RAD;
   mat2  rotate = mat2(cos(angle), -sin(angle),
                       sin(angle), cos(angle));

   gl_Position = uMVPMatrix * vec4(aVertex + rotate * aXYOffset, 0.0f, 1.0f);
}
