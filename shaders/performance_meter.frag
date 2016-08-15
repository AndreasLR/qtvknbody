#version 450

layout (location = 0) out vec4 outFragColor;

layout (location = 0) in vec2 inUv;

layout (std140, binding = 0) uniform UBO
{
//    float positions[8];
    vec4 hack_a;
    vec4 hack_b;
    int process_count;
    float relative_size;
} ubo;

void main()
{
    float positions[8];
    positions[0] = ubo.hack_a.x;
    positions[1] = ubo.hack_a.y;
    positions[2] = ubo.hack_a.z;
    positions[3] = ubo.hack_a.w;
    positions[4] = ubo.hack_b.x;
    positions[5] = ubo.hack_b.y;
    positions[6] = ubo.hack_b.z;
    positions[7] = ubo.hack_b.w;

    vec4 color = vec4(0.0,0.0,0.0,1.0);

    float process_position_low = 0;
    float process_position_high = 0;

    for ( int i = 0; i < ubo.process_count; i++)
    {
        process_position_high += positions[i];

        if ((inUv.x > process_position_low*ubo.relative_size) && (inUv.x < process_position_high*ubo.relative_size))
        {
            color.r += mod(float(i)*0.30, 1.0);
            color.g += mod(float(i)*0.30 + 0.33, 1.0);
            color.b += mod(float(i)*0.30 + 0.66, 1.0);
        }

        process_position_low = process_position_high;
    }

    outFragColor = color;
}
