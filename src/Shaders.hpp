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

void main() {
    vec3 normal = normalize(vNormal);
    float diffuse = max(dot(normal, normalize(uLightDirection)), 0.0);
    float rim = pow(1.0 - max(dot(normal, normalize(uCameraPosition - vWorldPosition)), 0.0), 2.0);
    vec3 lit = vColor * (0.22 + diffuse * 0.78) + uEmission + rim * uEmission * 0.35;

    float distanceToCamera = length(uCameraPosition - vWorldPosition);
    float fog = 1.0 - exp(-pow(distanceToCamera * uFogDensity, 2.0));
    vec3 color = mix(lit, uFogColor, clamp(fog, 0.0, 0.94));
    color = pow(max(color, vec3(0.0)), vec3(1.0 / 2.2));
    outColor = vec4(color, uOpacity);
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

float valueNoise(vec2 value) {
    vec2 whole = floor(value);
    vec2 fraction = fract(value);
    fraction = fraction * fraction * (3.0 - 2.0 * fraction);
    return mix(mix(hash21(whole), hash21(whole + vec2(1.0, 0.0)), fraction.x),
               mix(hash21(whole + vec2(0.0, 1.0)), hash21(whole + vec2(1.0, 1.0)), fraction.x),
               fraction.y);
}

void main() {
    vec3 low = vec3(0.015, 0.12, 0.15);
    vec3 high = vec3(0.006, 0.035, 0.12);
    float vertical = smoothstep(0.0, 0.93, vUv.y);
    vec3 sky = mix(low, high, vertical);

    float horizon = exp(-pow((vUv.y - 0.31) * 13.0, 2.0));
    float sunsetSide = smoothstep(0.27, 1.0, vUv.x);
    sky += vec3(0.92, 0.08, 0.035) * horizon * sunsetSide * 0.78;
    sky += vec3(0.13, 0.36, 0.5) * exp(-pow((vUv.y - 0.44) * 5.5, 2.0)) * 0.25;

    float clouds = valueNoise(vec2(vUv.x * 5.0 + uTime * 0.008, vUv.y * 17.0));
    clouds += 0.55 * valueNoise(vec2(vUv.x * 12.0 - uTime * 0.012, vUv.y * 29.0));
    float cloudBand = smoothstep(0.12, 0.83, vUv.y) * (1.0 - smoothstep(0.6, 0.96, vUv.y));
    sky -= vec3(0.012, 0.026, 0.045) * clouds * cloudBand;

    vec2 starGrid = vUv * vec2(180.0, 105.0);
    vec2 starCell = floor(starGrid);
    vec2 starPosition = fract(starGrid) - 0.5;
    float star = step(0.992, hash21(starCell)) * smoothstep(0.065, 0.0, length(starPosition));
    star *= smoothstep(0.39, 0.92, vUv.y);
    sky += vec3(0.3, 0.66, 1.0) * star;

    float grain = hash21(vUv * 900.0 + uTime) - 0.5;
    sky += grain * 0.018;
    outColor = vec4(max(sky, vec3(0.0)), 1.0);
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
