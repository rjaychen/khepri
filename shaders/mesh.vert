#version 450

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    mat4 model;
    vec4 baseColorFactor;
    int useTexture;
    float shininess;
    float specularStrength;
    float ambientStrength;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragUV;
layout(location = 2) out vec3 fragWorldPos;

void main() {
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);
    fragWorldPos = worldPos.xyz;

    gl_Position = pc.mvp * vec4(inPosition, 1.0);
    fragNormal = mat3(transpose(inverse(mat3(pc.model)))) * inNormal;
    fragUV = inUV;
}
