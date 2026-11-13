#version 330 core

// LUMEN vertex shader. To add behaviour, add a mode to cfg::Shading and a branch
// in the fragment shader.

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec3 aTangent;
layout(location = 4) in uvec4 aJoints;    // skinned meshes only
layout(location = 5) in vec4  aWeights;

// Skinning: each vertex follows up to four bones, mixed by its weights.
// MAX_JOINTS must match cfg::kMaxJoints.
const int MAX_JOINTS = 100;
uniform int  uSkinned;
uniform mat4 uJoints[MAX_JOINTS];

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
uniform mat3 uNormalMatrix;

// Gouraud lighting is worked out here, per vertex, then blended across the
// triangle. A small highlight can fall between vertices and disappear.
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
    // Inverse square, fading smoothly to zero at the range, so the lamp feels carried.
    float a = 1.0 / (1.0 + 0.22 * dist + 0.20 * dist * dist);
    float edge = clamp(1.0 - (dist / range) * (dist / range), 0.0, 1.0);
    return a * edge * edge;
}

void main() {
    vec4 localPos     = vec4(aPos, 1.0);
    vec3 localNormal  = aNormal;
    vec3 localTangent = aTangent;
    if (uSkinned == 1) {
        mat4 skin = aWeights.x * uJoints[aJoints.x] + aWeights.y * uJoints[aJoints.y] +
                    aWeights.z * uJoints[aJoints.z] + aWeights.w * uJoints[aJoints.w];
        localPos     = skin * localPos;
        localNormal  = mat3(skin) * aNormal;
        localTangent = mat3(skin) * aTangent;
    }

    vec4 worldPos = uModel * localPos;
    vWorldPos = worldPos.xyz;
    vNormal   = normalize(uNormalMatrix * localNormal);
    vUV       = aUV;

    vec3 T = normalize(uNormalMatrix * localTangent);
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
