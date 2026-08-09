#version 450

layout(set = 0, binding = 0) uniform sampler2D texSampler;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    mat4 model;
    vec4 baseColorFactor;
    int useTexture;
} pc;

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

    vec4 texColor = texture(texSampler, fragUV);
    vec3 baseColor = (pc.useTexture == 1) ? (texColor.rgb * pc.baseColorFactor.rgb) : pc.baseColorFactor.rgb;
    float alpha = (pc.useTexture == 1) ? (texColor.a * pc.baseColorFactor.a) : pc.baseColorFactor.a;

    outColor = vec4(baseColor * light, alpha);
}
