#version 450

layout(set = 0, binding = 0) uniform sampler2D texSampler;

struct LightData {
    vec4 position;   // xyz = pos, w = type (0 = Dir, 1 = Point, 2 = Spot)
    vec4 direction;  // xyz = dir, w = innerCutoff (cos)
    vec4 color;      // rgb = color, w = intensity
    vec4 params;     // x = constant, y = linear, z = quadratic, w = outerCutoff (cos)
};

layout(set = 0, binding = 1) uniform LightUBO {
    vec4 cameraPos;  // xyz = viewPos, w = numLights
    LightData lights[16];
} ubo;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    mat4 model;
    vec4 baseColorFactor;
    vec4 emissiveFactor;
    int useTexture;
    float shininess;
    float specularStrength;
    float ambientStrength;
} pc;

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragUV;
layout(location = 2) in vec3 fragWorldPos;

layout(location = 0) out vec4 outColor;

vec3 CalculateLight(LightData light, vec3 N, vec3 V, vec3 albedo) {
    int type = int(light.position.w + 0.5);
    vec3 lightColor = light.color.rgb;
    float intensity = light.color.w;

    vec3 L;
    float attenuation = 1.0;

    if (type == 0) {
        // Directional Light
        L = normalize(-light.direction.xyz);
    } else {
        // Point Light or Spot Light
        vec3 lightToPos = light.position.xyz - fragWorldPos;
        float dist = length(lightToPos);
        L = (dist > 0.0001) ? (lightToPos / dist) : vec3(0.0, 1.0, 0.0);

        // Distance Attenuation: 1.0 / (constant + linear * d + quadratic * d^2)
        float c = light.params.x;
        float lin = light.params.y;
        float quad = light.params.z;
        attenuation = 1.0 / (c + lin * dist + quad * dist * dist);

        if (type == 2) {
            // Spot Light Cone Angle Cutoff
            float theta = dot(L, normalize(-light.direction.xyz));
            float innerCutoff = light.direction.w;
            float outerCutoff = light.params.w;

            float epsilon = innerCutoff - outerCutoff;
            float spotIntensity = clamp((theta - outerCutoff) / max(epsilon, 0.0001), 0.0, 1.0);
            attenuation *= spotIntensity;
        }
    }

    // Diffuse Shading (Lambertian)
    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = diff * albedo * lightColor * intensity;

    // Specular Shading (Blinn-Phong)
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), max(pc.shininess, 1.0));
    vec3 specular = spec * pc.specularStrength * lightColor * intensity;

    return (diffuse + specular) * attenuation;
}

void main() {
    vec3 N = (length(fragNormal) > 0.001) ? normalize(fragNormal) : vec3(0.0, 1.0, 0.0);
    vec3 V = normalize(ubo.cameraPos.xyz - fragWorldPos);

    vec4 texColor = texture(texSampler, fragUV);
    vec3 albedo = (pc.useTexture == 1) ? (texColor.rgb * pc.baseColorFactor.rgb) : pc.baseColorFactor.rgb;
    float alpha = (pc.useTexture == 1) ? (texColor.a * pc.baseColorFactor.a) : pc.baseColorFactor.a;

    // Ambient Lighting
    vec3 ambient = pc.ambientStrength * albedo;

    // Self-Illumination / Emission (RGB color * scalar intensity in pc.emissiveFactor.w)
    vec3 emission = pc.emissiveFactor.rgb * pc.emissiveFactor.w;

    // Accumulate Light Contributions
    vec3 totalLighting = ambient;
    int numLights = int(clamp(ubo.cameraPos.w, 0.0, 16.0));

    // Fallback if no lights in scene
    if (numLights == 0) {
        vec3 defaultL = normalize(vec3(0.577, 0.577, 0.577));
        float diff = max(dot(N, defaultL), 0.0);
        totalLighting = (0.25 + diff * 0.75) * albedo;
    } else {
        for (int i = 0; i < numLights; ++i) {
            totalLighting += CalculateLight(ubo.lights[i], N, V, albedo);
        }
    }

    outColor = vec4(totalLighting + emission, alpha);
}
