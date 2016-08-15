#version 450

layout (std140, binding = 0) uniform UBO
{
    mat4 projectionMatrix;
    mat4 modelMatrix;
    mat4 viewMatrix;
    vec2 fbo_size;
    float timestamp;
    float timestep;
    float particle_size;
} ubo;
layout (binding = 1) uniform sampler2D samplerColorMap;
layout (binding = 2) uniform sampler2D samplerNoise;

layout (location = 0) in vec4 inXyzm;
layout (location = 1) in vec3 inVelocity;
layout (location = 2) in float inPointSize;
layout (location = 3) in vec2 inTexCoord;

layout (location = 0) out vec4 outFragColor;

void main () 
{
    vec2 xy = inTexCoord - vec2(0.5);
    float radius = length(xy);
    float velocity =  length(inVelocity);

    // Compute  color
    vec3 color_a = vec3(1.0,1.0,1.0);
    vec3 color_b = clamp(vec3(0.0,0.3,1.0) + vec3(1.0,-0.25,-0.7)*clamp(velocity*ubo.timestep*30.0,0.0,1.0), 0.0, 1.0);

    float x = clamp(radius*8.0, 0.0, 1.0);
    vec3 color_gradient = color_a * (1-x)  + color_b * x;

    float ang = atan(xy.x,xy.y);
    float noise_mass = texture(samplerNoise, vec2(clamp(inXyzm.w, 0.0, 10.0),0.5)).x;

    // Time intensity factor
    float factor_time= texture(samplerNoise, vec2(0.5, noise_mass * ubo.timestamp*0.01)).x + 1.0;

    // Intensity depends on the angle, resulting in rays
    float factor_angle = texture(samplerNoise, vec2(ang*0.01, noise_mass + ubo.timestamp*0.003)).x;

    // Angle intensity influence factor
    float factor_angle_influence = clamp(radius*2.0+0.05 ,0.0,1.0);

    // Distance intensity factor. The intensity varies with the radius, fading as it increases
    float factor_radius= pow(clamp((1-radius*2.0), 0.0, 1.0), 2.0);

    // Linear interpolation between computed and texture result. Use texture with mipmaps for small points to prevent flicker
    x = clamp(0.1 * inPointSize - 1, 0.0, 1.0);

    vec4 fragColorTexture = vec4(0.0);
    vec4 fragColorComputed = vec4(0.0);

    if (x < 1.0) fragColorTexture = vec4(color_gradient, 1.0) * texture(samplerColorMap, inTexCoord);
    if (x > 0.0) fragColorComputed = vec4(color_gradient,1.0) * factor_radius  * (factor_angle * factor_angle_influence * factor_time + 1.0 * (1.0 - factor_angle_influence));

    outFragColor = fragColorTexture*(1.0-x) + fragColorComputed*(x);
}
