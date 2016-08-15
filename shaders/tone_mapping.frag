#version 450

layout (binding = 0) uniform sampler2D tex;

layout (std140, binding = 1) uniform UBO
{
    float gamma;
    float exposure;
    int tone_mapping_method;
} ubo;

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;

void main()
{
    vec4 color = texture(tex, inUV);
    vec4 mapped;

    if (ubo.tone_mapping_method == 0)
    {
        // Reinhard tone mapping
        mapped = color / (color + vec4(1.0));
    }
    else if (ubo.tone_mapping_method == 1)
    {
        // Exposure tone mapping
        mapped = vec4(1.0) - exp(-color * ubo.exposure);
    }
    else if (ubo.tone_mapping_method == 2)
    {
        mapped = color;
    }

    // Gamma correction
    if (ubo.tone_mapping_method != 2)
    {
        mapped = pow(mapped, vec4(1.0 / ubo.gamma));
    }

    outFragColor = vec4(mapped.rgb, 1.0);
}
