#version 450

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    mat4 model;
    vec4 baseColorFactor;
    int useTexture;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragUV;

void main() {
    gl_Position = pc.mvp * vec4(inPosition, 1.0);
    // Pass world-space normal (top-left 3x3 of model matrix)
    fragNormal = mat3(transpose(inverse(mat3(pc.model)))) * inNormal;
    fragUV = inUV;
}
