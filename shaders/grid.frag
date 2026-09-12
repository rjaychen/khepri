#version 450

layout(push_constant) uniform GridPushConstants {
    mat4 viewProj;
    mat4 invViewProj;
    vec4 cameraPos;
    vec4 gridParams; // x = cellSize (e.g. 1.0), y = majorStep (e.g. 10.0), z = maxDistance (e.g. 100.0), w = opacity (e.g. 0.8)
} pc;

layout(location = 0) in vec3 nearPoint;
layout(location = 1) in vec3 farPoint;

layout(location = 0) out vec4 outColor;

// Computes 1D anti-aliased grid line coverage with subpixel filtering and Nyquist moire suppression
float ComputeGridLine(float coord, float dcoord, float lineWidthPixels) {
    float dist = abs(fract(coord - 0.5) - 0.5); // Distance to line center [0, 0.5] in grid units
    float distPixels = dist / max(dcoord, 1e-6); // Distance to line in screen pixel units
    float halfWidth = lineWidthPixels * 0.5;
    float coverage = clamp(halfWidth + 0.5 - distPixels, 0.0, 1.0);
    // Smoothly fade line when cell size approaches or is smaller than 1 screen pixel (Nyquist limit) to prevent moire
    float nyquistFade = clamp(2.0 - 2.0 * dcoord, 0.0, 1.0);
    return coverage * nyquistFade;
}

void main() {
    float dy = farPoint.y - nearPoint.y;
    if (abs(dy) < 1e-6) {
        discard;
    }

    // Intersect viewing ray with horizontal plane Y = 0
    float t = -nearPoint.y / dy;
    if (t <= 0.0) {
        discard;
    }

    vec3 fragWorldPos = nearPoint + t * (farPoint - nearPoint);

    // Compute clip depth for accurate hardware depth buffer interaction
    vec4 clipPos = pc.viewProj * vec4(fragWorldPos, 1.0);
    if (clipPos.w <= 0.0) {
        discard;
    }
    float depth = clipPos.z / clipPos.w;
    if (depth < 0.0 || depth > 1.0) {
        discard;
    }
    gl_FragDepth = depth;

    // Parameters
    float cellSize = pc.gridParams.x > 0.0 ? pc.gridParams.x : 1.0;
    float majorStep = pc.gridParams.y > 0.0 ? pc.gridParams.y : 10.0;
    float maxDist = pc.gridParams.z > 0.0 ? pc.gridParams.z : 100.0;
    float opacity = pc.gridParams.w > 0.0 ? pc.gridParams.w : 0.8;

    // Ground plane coordinates (X and Z)
    vec2 pos = fragWorldPos.xz;
    vec2 minorCoord = pos / cellSize;
    vec2 majorCoord = pos / (cellSize * majorStep);

    vec2 dMinor = fwidth(minorCoord);
    vec2 dMajor = fwidth(majorCoord);
    vec2 dWorld = fwidth(pos);

    // 1. Minor Grid Lines (Subtle cool light-slate gray)
    float minorX = ComputeGridLine(minorCoord.x, dMinor.x, 1.2);
    float minorZ = ComputeGridLine(minorCoord.y, dMinor.y, 1.2);
    float minorCoverage = max(minorX, minorZ);

    // 2. Major Grid Lines (Clean bright silver-white)
    float majorX = ComputeGridLine(majorCoord.x, dMajor.x, 1.4);
    float majorZ = ComputeGridLine(majorCoord.y, dMajor.y, 1.4);
    float majorCoverage = max(majorX, majorZ);

    // 3. Coordinate Axes (Vibrant Red X-axis and Electric Blue Z-axis)
    // Z-axis runs along X = 0 (fragWorldPos.x == 0)
    float zAxisDist = abs(fragWorldPos.x) / max(dWorld.x, 1e-6);
    float zAxisCoverage = clamp(1.4 - zAxisDist, 0.0, 1.0);

    // X-axis runs along Z = 0 (fragWorldPos.z == 0)
    float xAxisDist = abs(fragWorldPos.z) / max(dWorld.y, 1e-6);
    float xAxisCoverage = clamp(1.4 - xAxisDist, 0.0, 1.0);

    // 4. Color Assembly (Modern clean aesthetic for blue-gray editor background)
    vec4 minorCol = vec4(0.62, 0.67, 0.75, minorCoverage * 0.35);
    vec4 majorCol = vec4(0.88, 0.90, 0.96, majorCoverage * 0.70);

    // Blend minor and major lines
    vec4 gridCol = mix(minorCol, majorCol, majorCol.a);

    // Overlay Coordinate Axes (Z-axis = Vibrant Blue, X-axis = Vibrant Red)
    if (zAxisCoverage > 0.0) {
        vec4 zAxisCol = vec4(0.26, 0.58, 1.00, zAxisCoverage * 0.95);
        gridCol = mix(gridCol, zAxisCol, zAxisCol.a);
    }
    if (xAxisCoverage > 0.0) {
        vec4 xAxisCol = vec4(0.96, 0.26, 0.26, xAxisCoverage * 0.95);
        gridCol = mix(gridCol, xAxisCol, xAxisCol.a);
    }

    // Distance falloff fade towards horizon
    float dist = length(fragWorldPos.xz - pc.cameraPos.xz);
    float fade = clamp(1.0 - (dist / maxDist), 0.0, 1.0);
    fade = fade * fade; // Smooth quadratic falloff

    gridCol.a *= fade * opacity;

    if (gridCol.a < 0.002) {
        discard;
    }

    outColor = gridCol;
}
