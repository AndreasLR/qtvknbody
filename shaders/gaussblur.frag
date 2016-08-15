#version 450

layout (binding = 0) uniform sampler2D blur_source;

layout (std140, binding = 1) uniform UBO
{
    float blur_extent;
    float blur_strength;
} ubo;

layout (push_constant) uniform PushConsts
{
    int horizontal;
} pushConsts;

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;

void main() 
{
    // Compute the number of samples required for an alias free blur, but limit samples to a reasonable number
    vec2 blur_source_size = vec2(textureSize(blur_source, 0));

    float sampling_extent = ubo.blur_extent * blur_source_size.y;
    int sample_count_tail = clamp(int(ceil(sampling_extent)), 0, 100);

    vec3 result = texture(blur_source, inUV).rgb * ubo.blur_strength;

    // Blur if more than zero samples
    if (sample_count_tail > 0)
    {
        // Compute the sampling interval corresponding to the blur extent and number of samples
        vec2 sampling_interval;
        sampling_interval.y = ubo.blur_extent / float(sample_count_tail);
        sampling_interval.x = sampling_interval.y * ( blur_source_size.y/ blur_source_size.x);

        // Compute Gaussian weight factors
        // Todo: compute weights once on host instead of for every shader invocation
        float sigma2 = 0.1;
        float a = 30.0/sqrt(2.0*sigma2*3.14159) / float(1 + 2*sample_count_tail);
        float b = 1.0/(2.0*sigma2);

        result *= a;

        // Perform actual blur
        if (pushConsts.horizontal == 1)
        {
            for(int i = 1; i < 1+sample_count_tail; ++i)
            {
                float pos = float(i)/float(sample_count_tail);
                float weight = a*exp(-pos*pos*b);

                result += texture(blur_source, inUV + vec2(sampling_interval.x * (float(i)), 0.0)).rgb * weight * ubo.blur_strength;
                result += texture(blur_source, inUV - vec2(sampling_interval.x * (float(i)), 0.0)).rgb * weight * ubo.blur_strength;
            }
        }
        else
        {
            for(int i = 1; i < 1+sample_count_tail; ++i)
            {
                float pos = float(i)/float(sample_count_tail);
                float weight = a*exp(-pos*pos*b);

                result += texture(blur_source, inUV + vec2(0.0, sampling_interval.y * (float(i)))).rgb * weight * ubo.blur_strength;
                result += texture(blur_source, inUV - vec2(0.0, sampling_interval.y * (float(i)))).rgb * weight * ubo.blur_strength;
            }
        }
    }

    outFragColor = vec4(result, 1.0);
}
