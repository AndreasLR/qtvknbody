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


// Instanced attributes
layout (location = 0) in vec4 xyzm;
layout (location = 1) in vec4 velocity;

// Vertex attribute
layout (location = 2) in vec2 corner;

// Out
layout (location = 0) out vec4 outXyzm;
layout (location = 1) out vec3 outVelocity;
layout (location = 2) out float outPointSize;
layout (location = 3) out vec2 outTexCoord;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main () 
{
    // Output
    outVelocity = velocity.xyz;
    outXyzm = xyzm;
    outTexCoord = corner;

    // Compute position
    vec4 position = ubo.viewMatrix * ubo.modelMatrix * vec4(xyzm.xyz, 1.0);
    vec4 midPos = position;

    float mass = xyzm.w;
    float radius = pow(mass, 1.0/3.0) * 0.05 * (ubo.particle_size * 0.05);
    position.xy += (corner - vec2(0.5)) * radius;
    gl_Position = ubo.projectionMatrix * position;

	

    // Compute point size in pixel to pass to fragment shader
    vec4 position_projected = ubo.projectionMatrix * vec4(radius, radius, -length(midPos.xyz/midPos.w), 1.0);
    vec2 projSize = ubo.fbo_size * position_projected.xy / position_projected.w;

    outPointSize = (projSize.x+projSize.y);
}
