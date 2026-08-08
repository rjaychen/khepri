#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

void main() {
    // Two-sided Lambertian diffuse + ambient lighting
    vec3 N = length(fragNormal) > 0.001 ? normalize(fragNormal) : vec3(0.0, 1.0, 0.0);
    vec3 L1 = normalize(vec3(0.577, 0.577, 0.577));
    vec3 L2 = normalize(vec3(-0.577, 0.577, -0.577));

    float diff1 = max(dot(N, L1), 0.0);
    float diff2 = max(dot(N, L2), 0.0) * 0.35;
    float ambient = 0.25;
    float light = ambient + diff1 * 0.75 + diff2;

    vec3 baseColor = vec3(0.35, 0.65, 0.95);  // Vibrant blue-cyan surface
    outColor = vec4(baseColor * light, 1.0);
}
