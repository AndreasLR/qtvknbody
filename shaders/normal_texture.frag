#version 450

layout (binding = 0) uniform sampler2D tex;

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;

void main() 
{
    outFragColor = texture(tex, inUV);
}
