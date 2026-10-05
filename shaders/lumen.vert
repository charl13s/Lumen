#version 330 core

// LUMEN - the one vertex shader. There is no second one. If you need different
// behaviour, add a mode to cfg::Shading and branch in the fragment shader.

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec3 aTangent;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
uniform mat3 uNormalMatrix;

// Gouraud is computed HERE, per vertex, which is the whole point of it: the
// lighting is evaluated at three corners and interpolated across the triangle.
// On a large wall quad with a small moving highlight, the highlight can land
// between the corners and disappear. That artefact is the lesson - see
// docs/LIGHTING.md and Gallery Four's technique notes.
uniform int   uShading;         // matches cfg::Shading
uniform vec3  uLampPos;
uniform vec3  uLampDir;
uniform vec3  uLampColour;
uniform float uLampIntensity;
uniform float uLampRange;
uniform float uLampInnerCos;
uniform float uLampOuterCos;
uniform vec3  uViewPos;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUV;
out mat3 vTBN;
out vec3 vGouraud;

const int SHADING_GOURAUD = 2;

float attenuation(float dist, float range) {
    // Inverse-square, with a smooth cut to zero at the range. Without this the
    // lamp lights the whole gallery evenly and stops reading as something
    // carried - it is the single most important line in the lighting.
    float a = 1.0 / (1.0 + 0.22 * dist + 0.20 * dist * dist);
    float edge = clamp(1.0 - (dist / range) * (dist / range), 0.0, 1.0);
    return a * edge * edge;
}

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;
    vNormal   = normalize(uNormalMatrix * aNormal);
    vUV       = aUV;

    vec3 T = normalize(uNormalMatrix * aTangent);
    vec3 N = vNormal;
    T = normalize(T - dot(T, N) * N);          // Gram-Schmidt
    vec3 B = cross(N, T);
    vTBN = mat3(T, B, N);

    vGouraud = vec3(0.0);
    if (uShading == SHADING_GOURAUD) {
        vec3 toLight = uLampPos - vWorldPos;
        float dist   = length(toLight);
        vec3  L      = toLight / max(dist, 0.0001);

        float theta = dot(-L, normalize(uLampDir));
        float cone  = clamp((theta - uLampOuterCos) /
                            max(uLampInnerCos - uLampOuterCos, 0.0001), 0.0, 1.0);

        float diff = max(dot(N, L), 0.0);
        vec3  V    = normalize(uViewPos - vWorldPos);
        vec3  H    = normalize(L + V);
        float spec = pow(max(dot(N, H), 0.0), 32.0);

        vGouraud = uLampColour * uLampIntensity *
                   attenuation(dist, uLampRange) * cone * (diff + 0.25 * spec);
    }

    gl_Position = uProj * uView * worldPos;
}
