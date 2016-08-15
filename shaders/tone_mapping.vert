#version 450

/*
 * Shader draws a HDR texture into a normal LDR buffer using tone mapping (from high to low dynamic range)
 * */

layout (location = 0) in vec2 inPos;
layout (location = 1) in vec2 inUV;

layout (location = 0) out vec2 outUV;

out gl_PerVertex 
{
    vec4 gl_Position;   
};

void main() 
{
    outUV = inUV;

    gl_Position = vec4(inPos.xy, 0.0, 1.0);
}
