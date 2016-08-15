#version 450

/*
 * Shader that draws a performance meter showing the relative time spent on various processes.
 * */

layout (location = 0) in vec2 inPos;
layout (location = 1) in vec2 inUv;

layout (std140, binding = 0) uniform UBO
{
//    float positions[8];
    vec4 hack_a;
    vec4 hack_b;
    int process_count;
    float relative_size;
} ubo;

layout (location = 0) out vec2 outUv;

out gl_PerVertex 
{
    vec4 gl_Position;   
};

void main() 
{
    outUv = inUv;
    gl_Position = vec4(inPos, 0.0, 1.0);
}
