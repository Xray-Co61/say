#pragma once

namespace fallen::shaders {

inline constexpr char worldVertex[] = R"GLSL(
#version 330 core
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aColor;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vWorldPosition;
out vec3 vNormal;
out vec3 vColor;

void main() {
    vec4 worldPosition = uModel * vec4(aPosition, 1.0);
    vWorldPosition = worldPosition.xyz;
    vNormal = mat3(transpose(inverse(uModel))) * aNormal;
    vColor = aColor;
    gl_Position = uProjection * uView * worldPosition;
}
)GLSL";

inline constexpr char worldFragment[] = R"GLSL(
#version 330 core
in vec3 vWorldPosition;
in vec3 vNormal;
in vec3 vColor;

uniform vec3 uCameraPosition;
uniform vec3 uFogColor;
uniform vec3 uLightDirection;
uniform vec3 uEmission;
uniform float uFogDensity;
uniform float uOpacity;

out vec4 outColor;

float hash21(vec2 value) {
    value = fract(value * vec2(123.34, 456.21));
    value += dot(value, value + 45.32);
    return fract(value.x * value.y);
}

void main() {
    vec3 normal = normalize(vNormal);
    float diffuse = max(dot(normal, normalize(uLightDirection)), 0.0);
    float slope = 1.0 - max(normal.y, 0.0);
    float mottling = hash21(floor(vWorldPosition.xz * 0.18)) - 0.5;
    vec3 earth = vColor * (0.18 + diffuse * 0.54);
    earth *= mix(1.0, 0.55, slope);
    earth += mottling * vec3(0.008, 0.016, 0.014);
    vec3 lit = earth + uEmission;

    float distanceToCamera = length(uCameraPosition - vWorldPosition);
    float fog = 1.0 - exp(-distanceToCamera * uFogDensity);
    fog = clamp(fog, 0.0, 0.985);
    vec3 color = mix(lit, uFogColor, fog);
    outColor = vec4(max(color, vec3(0.0)), uOpacity);
}
)GLSL";

inline constexpr char skyVertex[] = R"GLSL(
#version 330 core
layout (location = 0) in vec3 aPosition;
out vec2 vUv;

void main() {
    vUv = aPosition.xy * 0.5 + 0.5;
    gl_Position = vec4(aPosition.xy, 0.0, 1.0);
}
)GLSL";

inline constexpr char skyFragment[] = R"GLSL(
#version 330 core
in vec2 vUv;
uniform float uTime;
out vec4 outColor;

float hash21(vec2 value) {
    value = fract(value * vec2(123.34, 456.21));
    value += dot(value, value + 45.32);
    return fract(value.x * value.y);
}

float noise(vec2 value) {
    vec2 whole = floor(value);
    vec2 fraction = fract(value);
    fraction = fraction * fraction * (3.0 - 2.0 * fraction);
    return mix(mix(hash21(whole), hash21(whole + vec2(1.0, 0.0)), fraction.x),
               mix(hash21(whole + vec2(0.0, 1.0)), hash21(whole + vec2(1.0, 1.0)), fraction.x),
               fraction.y);
}

float fbm(vec2 value) {
    float total = 0.0;
    float amplitude = 0.5;
    for (int octave = 0; octave < 5; ++octave) {
        total += noise(value) * amplitude;
        value = value * 2.04 + vec2(17.1, 9.2);
        amplitude *= 0.5;
    }
    return total;
}

void main() {
    vec2 warped = vUv;
    warped.x += (fbm(vUv * 2.7 + vec2(uTime * 0.006, 0.0)) - 0.5) * 0.12;
    float cloudA = fbm(vec2(warped.x * 2.1 - uTime * 0.005, warped.y * 7.2));
    float cloudB = fbm(vec2(warped.x * 5.4 + uTime * 0.009, warped.y * 15.0));
    float horizon = exp(-pow((vUv.y - 0.36) * 7.8, 2.0));

    vec3 deepWater = vec3(0.003, 0.020, 0.024);
    vec3 murkyTeal = vec3(0.008, 0.115, 0.106);
    vec3 sky = mix(murkyTeal, deepWater, smoothstep(0.10, 0.94, vUv.y));
    sky += vec3(0.015, 0.080, 0.072) * horizon;
    sky -= vec3(0.009, 0.030, 0.028) * cloudA;
    sky += vec3(0.003, 0.018, 0.017) * (cloudB - 0.5);

    float coldGlow = exp(-pow((vUv.x - 0.18) * 2.2, 2.0) - pow((vUv.y - 0.72) * 3.3, 2.0));
    sky += vec3(0.010, 0.042, 0.048) * coldGlow;
    outColor = vec4(max(sky, vec3(0.0)), 1.0);
}
)GLSL";

inline constexpr char birdVertex[] = R"GLSL(
#version 330 core
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aColor;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vWorldPosition;
out vec3 vNormal;
out vec3 vColor;
out vec3 vLocalPosition;

void main() {
    vec4 worldPosition = uModel * vec4(aPosition, 1.0);
    vWorldPosition = worldPosition.xyz;
    vNormal = mat3(transpose(inverse(uModel))) * aNormal;
    vColor = aColor;
    vLocalPosition = aPosition;
    gl_Position = uProjection * uView * worldPosition;
}
)GLSL";

inline constexpr char birdFragment[] = R"GLSL(
#version 330 core
in vec3 vWorldPosition;
in vec3 vNormal;
in vec3 vColor;
in vec3 vLocalPosition;

uniform vec3 uCameraPosition;
uniform vec3 uFogColor;
uniform vec3 uLightDirection;
uniform float uUnderView;
uniform float uDisruption;
uniform float uTime;

out vec4 outColor;

float hash31(vec3 value) {
    value = fract(value * 0.1031);
    value += dot(value, value.yzx + 33.33);
    return fract((value.x + value.y) * value.z);
}

void main() {
    vec3 normal = normalize(vNormal);
    if (!gl_FrontFacing) {
        normal = -normal;
    }
    vec3 light = normalize(uLightDirection);
    vec3 viewDirection = normalize(uCameraPosition - vWorldPosition);
    float diffuse = max(dot(normal, light), 0.0);
    float backLight = pow(max(dot(-normal, light), 0.0), 2.1);
    float rim = pow(1.0 - max(dot(normal, viewDirection), 0.0), 3.0);

    // Fine per-feather variation without a texture: enough irregularity to avoid a plastic surface.
    float fiber = hash31(floor(vLocalPosition * 18.0 + vec3(0.0, uTime * 0.16, 0.0)));
    float featherLight = smoothstep(0.24, 0.92, abs(vLocalPosition.x) / 4.4);
    vec3 underColor = mix(vec3(0.075, 0.20, 0.17), vec3(0.56, 0.82, 0.70), featherLight);
    vec3 topColor = vColor * (0.18 + diffuse * 0.74) + vec3(0.05, 0.17, 0.13) * backLight;
    vec3 color = mix(topColor, underColor * (0.35 + diffuse * 0.45), uUnderView);
    color += vec3(0.015, 0.055, 0.042) * rim;
    color += (fiber - 0.5) * vec3(0.025, 0.050, 0.038);
    color = mix(color, vec3(0.90, 0.07, 0.018), clamp(uDisruption * 0.88, 0.0, 1.0));

    float distanceToCamera = length(uCameraPosition - vWorldPosition);
    float fog = 1.0 - exp(-distanceToCamera * 0.0027);
    color = mix(color, uFogColor, clamp(fog, 0.0, 0.92));
    outColor = vec4(max(color, vec3(0.0)), 1.0);
}
)GLSL";

inline constexpr char angelVertex[] = R"GLSL(
#version 330 core
layout (location = 0) in vec3 aPosition;

uniform mat4 uView;
uniform mat4 uProjection;
uniform vec3 uCenter;
uniform vec3 uCameraRight;
uniform vec3 uCameraUp;
uniform float uScale;

out vec2 vUv;

void main() {
    vec2 corner = aPosition.xy;
    vec3 worldPosition = uCenter
        + uCameraRight * corner.x * uScale * 2.05
        + uCameraUp * corner.y * uScale * 1.65;
    vUv = vec2(corner.x * 0.5 + 0.5, 1.0 - (corner.y * 0.5 + 0.5));
    gl_Position = uProjection * uView * vec4(worldPosition, 1.0);
}
)GLSL";

inline constexpr char angelFragment[] = R"GLSL(
#version 330 core
in vec2 vUv;

uniform vec3 uFogColor;
uniform float uDistance;
uniform float uDisruption;
uniform float uTime;
uniform float uSeed;
uniform float uBelowAngle;

out vec4 outColor;

float hash21(vec2 value) {
    value = fract(value * vec2(123.34, 456.21));
    value += dot(value, value + 45.32);
    return fract(value.x * value.y);
}

float segmentMask(vec2 point, vec2 start, vec2 end, float radius) {
    vec2 segment = end - start;
    float projection = clamp(dot(point - start, segment) / max(dot(segment, segment), 0.0001), 0.0, 1.0);
    float distanceToSegment = length(point - (start + segment * projection));
    return 1.0 - smoothstep(radius * 0.42, radius, distanceToSegment);
}

float ellipseMask(vec2 point, vec2 center, vec2 radii) {
    return 1.0 - smoothstep(0.78, 1.0, length((point - center) / radii));
}

void main() {
    // The signal is deliberately procedural: distant feather-like light, not a pasted image.
    vec2 point = vUv * 2.0 - 1.0;
    point.x *= 0.92;
    // Looking upward from below compresses the body and opens the wings into a broad underwing shape.
    point.y = mix(point.y, point.y * 0.72 + 0.08, uBelowAngle);
    float flap = sin(uTime * 2.4 + uSeed) * 0.075;

    float body = ellipseMask(point, vec2(0.0, 0.13), vec2(0.135, 0.25));
    float head = ellipseMask(point, vec2(0.0, -0.17), vec2(0.080, 0.070));
    float tail = segmentMask(point, vec2(0.0, 0.27), vec2(0.0, 0.68), 0.14);
    float wings = 0.0;

    for (int sideIndex = 0; sideIndex < 2; ++sideIndex) {
        float side = sideIndex == 0 ? -1.0 : 1.0;
        vec2 root = vec2(side * 0.045, 0.01);
        for (int featherIndex = 0; featherIndex < 10; ++featherIndex) {
            float fraction = float(featherIndex) / 9.0;
            float frontArc = -0.38 - fraction * 0.52;
            float underArc = -0.13 - fraction * 0.39;
            float arc = mix(frontArc, underArc, uBelowAngle) + flap * (0.25 + fraction * 0.55);
            vec2 tip = vec2(side * (0.25 + fraction * 0.79), arc);
            float width = mix(0.155, 0.042, fraction);
            wings = max(wings, segmentMask(point, root, tip, width));
        }
        // Translucent wing membrane makes individual feather lines read as a single distant silhouette.
        wings = max(wings, ellipseMask(point, vec2(side * 0.36, mix(-0.18, -0.05, uBelowAngle) + flap * 0.3), vec2(0.50, mix(0.38, 0.28, uBelowAngle))) * 0.42);
    }

    float signal = max(max(body, head), max(tail, wings));
    float particulate = hash21(floor(point * 115.0) + floor(uTime * 19.0) + uSeed);
    signal *= smoothstep(0.08, 0.95, particulate + signal * 0.42);

    float atmosphericFade = exp(-uDistance * 0.00058);
    float alpha = signal * mix(0.22, 0.92, atmosphericFade);
    float core = max(body, head);
    float featherTip = smoothstep(0.12, 0.94, abs(point.x));
    vec3 underWing = mix(vec3(0.045, 0.16, 0.14), vec3(0.60, 0.88, 0.76), featherTip);
    vec3 normalWing = mix(vec3(0.20, 0.54, 0.48), vec3(0.92, 1.0, 0.90), core * 0.92 + wings * 0.35);
    vec3 color = mix(normalWing, underWing + vec3(core * 0.24), uBelowAngle * wings);
    color = mix(color, vec3(0.82, 0.075, 0.018), clamp(uDisruption * 0.86, 0.0, 1.0));

    float fog = 1.0 - exp(-uDistance * 0.0011);
    color = mix(color, uFogColor, fog * 0.45);
    outColor = vec4(color, alpha);
}
)GLSL";

inline constexpr char projectileFragment[] = R"GLSL(
#version 330 core
in vec2 vUv;

uniform float uTime;
uniform float uAge;
out vec4 outColor;

float hash21(vec2 value) {
    value = fract(value * vec2(123.34, 456.21));
    value += dot(value, value + 45.32);
    return fract(value.x * value.y);
}

void main() {
    vec2 point = vUv * 2.0 - 1.0;
    float radius = length(point);
    float shell = 1.0 - smoothstep(0.42, 0.98, radius);
    float core = 1.0 - smoothstep(0.0, 0.30, radius);
    float turbulence = hash21(floor(point * 34.0) + floor((uTime + uAge) * 48.0));
    shell *= smoothstep(0.12, 0.96, turbulence + 0.38);

    vec3 redCore = vec3(1.0, 0.08, 0.012);
    vec3 ember = vec3(1.0, 0.28, 0.035);
    vec3 color = mix(redCore, ember, core) * (0.55 + core * 0.85);
    outColor = vec4(color, shell * 0.92);
}
)GLSL";

inline constexpr char postVertex[] = R"GLSL(
#version 330 core
layout (location = 0) in vec3 aPosition;
out vec2 vUv;

void main() {
    vUv = aPosition.xy * 0.5 + 0.5;
    gl_Position = vec4(aPosition.xy, 0.0, 1.0);
}
)GLSL";

inline constexpr char postFragment[] = R"GLSL(
#version 330 core
in vec2 vUv;
uniform sampler2D uScene;
uniform float uTime;
uniform float uResolutionX;
uniform float uResolutionY;
uniform float uZoom;
uniform float uKick;
out vec4 outColor;

float hash21(vec2 value) {
    value = fract(value * vec2(123.34, 456.21));
    value += dot(value, value + 45.32);
    return fract(value.x * value.y);
}

vec3 sampleScene(vec2 uv) {
    return texture(uScene, clamp(uv, 0.001, 0.999)).rgb;
}

void main() {
    vec2 texel = vec2(1.0 / uResolutionX, 1.0 / uResolutionY);
    vec2 centered = vUv - 0.5;
    float radius = dot(centered, centered);
    vec2 aberration = centered * (0.0013 + 0.00045 * uZoom);

    vec3 color;
    color.r = sampleScene(vUv + aberration).r;
    color.g = sampleScene(vUv).g;
    color.b = sampleScene(vUv - aberration).b;

    // Short recoil smear gives each shell a physical, heavy optical response.
    vec2 kickOffset = vec2(0.0, uKick * 0.0075);
    vec3 recoilBlur = (sampleScene(vUv + kickOffset) + sampleScene(vUv + kickOffset * 0.45) + color) / 3.0;
    color = mix(color, recoilBlur, clamp(uKick * 0.42, 0.0, 0.42));

    // Small, restrained bloom is what turns the white signal into a filmed light source.
    vec3 blur = vec3(0.0);
    blur += sampleScene(vUv + texel * vec2(-2.0, 0.0));
    blur += sampleScene(vUv + texel * vec2(2.0, 0.0));
    blur += sampleScene(vUv + texel * vec2(0.0, -2.0));
    blur += sampleScene(vUv + texel * vec2(0.0, 2.0));
    blur += sampleScene(vUv + texel * vec2(-1.0, -1.0));
    blur += sampleScene(vUv + texel * vec2(1.0, 1.0));
    blur /= 6.0;
    vec3 bloom = max(blur - vec3(0.09), vec3(0.0));
    color += bloom * 0.72;

    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(luminance) * vec3(0.58, 0.95, 0.86), color, 0.52);
    color = color / (color + vec3(0.48));
    color = pow(max(color, vec3(0.0)), vec3(1.0 / 1.85));

    float grain = hash21(gl_FragCoord.xy + vec2(uTime * 71.0, uTime * 19.0)) - 0.5;
    color += grain * 0.070;
    float vignette = smoothstep(0.76, 0.12, radius * 1.72);
    color *= mix(0.42, 1.0, vignette);
    outColor = vec4(max(color, vec3(0.0)), 1.0);
}
)GLSL";

inline constexpr char hudVertex[] = R"GLSL(
#version 330 core
layout (location = 0) in vec2 aPosition;
layout (location = 1) in vec4 aColor;
out vec4 vColor;

void main() {
    vColor = aColor;
    gl_Position = vec4(aPosition, 0.0, 1.0);
}
)GLSL";

inline constexpr char hudFragment[] = R"GLSL(
#version 330 core
in vec4 vColor;
out vec4 outColor;

void main() {
    outColor = vColor;
}
)GLSL";

} // namespace fallen::shaders
