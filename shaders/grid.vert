#version 450

layout(push_constant) uniform GridPushConstants {
    mat4 viewProj;
    mat4 invViewProj;
    vec4 cameraPos;
    vec4 gridParams; // x = cellSize, y = majorStep, z = maxDistance, w = opacity
} pc;

layout(location = 0) out vec3 nearPoint;
layout(location = 1) out vec3 farPoint;

vec3 UnprojectPoint(float x, float y, float z, mat4 invViewProj) {
    vec4 unprojectedPoint = invViewProj * vec4(x, y, z, 1.0);
    return unprojectedPoint.xyz / unprojectedPoint.w;
}

// 6 vertices of a quad covering [-1, 1] in NDC clip space
const vec3 gridPlane[6] = vec3[](
    vec3(-1.0, -1.0, 0.0), vec3( 1.0, -1.0, 0.0), vec3( 1.0,  1.0, 0.0),
    vec3(-1.0, -1.0, 0.0), vec3( 1.0,  1.0, 0.0), vec3(-1.0,  1.0, 0.0)
);

void main() {
    vec3 p = gridPlane[gl_VertexIndex];
    // In Vulkan standard clip space, near plane is z = 0.0, far plane is z = 1.0
    nearPoint = UnprojectPoint(p.x, p.y, 0.0, pc.invViewProj);
    farPoint  = UnprojectPoint(p.x, p.y, 1.0, pc.invViewProj);
    gl_Position = vec4(p, 1.0);
}
