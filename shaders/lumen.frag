#version 330 core

// LUMEN fragment shader.
//
// Two lights:
//   The lamp   a cone the visitor carries. Hard light, falls off fast, casts
//              shadows from a shadow map rendered from the lamp.
//   The vault  soft light from above, only in the room the visitor is in.
//
// Lighting is done in linear space. The last line converts back for display;
// do not add a second gamma correction.

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUV;
in mat3 vTBN;
in vec3 vGouraud;

out vec4 FragColor;

uniform sampler2D uAlbedo;
uniform sampler2D uNormalMap;

uniform vec3  uTint;
uniform float uSpecular;
uniform float uShininess;
uniform float uEmissive;
uniform float uTriplanarScale;  // texture repeats per metre
uniform int   uShading;

uniform vec3  uLampPos;
uniform vec3  uLampDir;
uniform vec3  uLampColour;
uniform float uLampIntensity;
uniform float uLampRange;
uniform float uLampInnerCos;
uniform float uLampOuterCos;

uniform vec3  uVaultColour;
uniform float uVaultLevel;      // 0 to the reveal level
uniform vec3  uViewPos;

uniform float uAmbient;         // small, so nothing is pure black

uniform sampler2DShadow uShadowMap;
uniform mat4  uLampViewProj;
uniform int   uShadowsOn;
uniform int   uShadowPass;      // 1 while drawing the shadow map: depth only
uniform float uShadowBias;
uniform float uShadowNormalPush;

const int SHADING_LAMBERT       = 0;
const int SHADING_BLINN_PHONG   = 1;
const int SHADING_GOURAUD       = 2;
const int SHADING_NORMAL_MAPPED = 3;
const int SHADING_EMISSIVE      = 4;
const int SHADING_TRIPLANAR     = 5;

float attenuation(float dist, float range) {
    float a = 1.0 / (1.0 + 0.22 * dist + 0.20 * dist * dist);
    float edge = clamp(1.0 - (dist / range) * (dist / range), 0.0, 1.0);
    return a * edge * edge;
}

// How much lamp light reaches this point: 1 lit, 0 in shadow. Projects the
// point into the lamp's view and compares its depth with what the lamp saw,
// averaging 9 nearby lookups (PCF) for a soft edge.
float lampShadow(vec3 geomNormal) {
    if (uShadowsOn == 0) return 1.0;
    vec4 ls = uLampViewProj * vec4(vWorldPos + geomNormal * uShadowNormalPush, 1.0);
    if (ls.w <= 0.0) return 1.0;
    vec3 c = ls.xyz / ls.w * 0.5 + 0.5;
    if (c.z > 1.0) return 1.0;

    vec2 texel = 1.0 / vec2(textureSize(uShadowMap, 0));
    float sum = 0.0;
    for (int y = -1; y <= 1; ++y)
        for (int x = -1; x <= 1; ++x)
            sum += texture(uShadowMap, vec3(c.xy + vec2(x, y) * texel, c.z - uShadowBias));
    return sum / 9.0;
}

void main() {
    if (uShadowPass == 1) {     // only depth is wanted
        FragColor = vec4(0.0);
        return;
    }

    vec4 albedoSample = texture(uAlbedo, vUV);

    // Triplanar: for a scan with no texture coordinates, project the texture
    // along x, y and z and blend the three by how much the surface faces each.
    if (uShading == SHADING_TRIPLANAR) {
        vec3 w = pow(abs(normalize(vNormal)), vec3(4.0));
        w /= (w.x + w.y + w.z);
        vec3 p = vWorldPos * uTriplanarScale;
        albedoSample = texture(uAlbedo, p.zy) * w.x +
                       texture(uAlbedo, p.xz) * w.y +
                       texture(uAlbedo, p.xy) * w.z;
    }
    vec3 albedo = albedoSample.rgb * uTint;

    // Surface normal, bumped by the normal map if there is one.
    vec3 N = normalize(vNormal);
    // A triplanar statue can still carry its scan's normal map; without one the
    // flat placeholder leaves the normal unchanged.
    if (uShading == SHADING_NORMAL_MAPPED || uShading == SHADING_TRIPLANAR) {
        vec3 sampled = texture(uNormalMap, vUV).rgb * 2.0 - 1.0;
        N = normalize(vTBN * sampled);
    }

    vec3 V = normalize(uViewPos - vWorldPos);

    // ---- the carried lamp --------------------------------------------------
    vec3 toLight = uLampPos - vWorldPos;
    float dist   = length(toLight);
    vec3  L      = toLight / max(dist, 0.0001);

    // Cone falloff.
    float theta = dot(-L, normalize(uLampDir));
    float cone  = clamp((theta - uLampOuterCos) /
                        max(uLampInnerCos - uLampOuterCos, 0.0001), 0.0, 1.0);
    cone = cone * cone;      // soften the cone edge

    float shadow = lampShadow(normalize(vNormal));
    float atten  = attenuation(dist, uLampRange) * cone * shadow;

    float diff = max(dot(N, L), 0.0);
    vec3  lamp = uLampColour * uLampIntensity * atten * diff;

    if (uShading == SHADING_BLINN_PHONG || uShading == SHADING_NORMAL_MAPPED ||
        uShading == SHADING_TRIPLANAR) {
        // Blinn-Phong: uses the halfway vector instead of the reflection vector.
        vec3  H    = normalize(L + V);
        float spec = pow(max(dot(N, H), 0.0), uShininess);
        lamp += uLampColour * uLampIntensity * atten * spec * uSpecular;
    }

    // ---- the vault wash ----------------------------------------------------
    // Soft light from above, modelled as a hemisphere rather than a point.
    float sky   = 0.5 + 0.5 * dot(N, vec3(0.0, 1.0, 0.0));
    vec3  vault = uVaultColour * uVaultLevel * sky;

    // A faint wrap so vertical surfaces under the vault are not black.
    vault += uVaultColour * uVaultLevel * 0.25;

    // ---- combine -----------------------------------------------------------
    vec3 lit;
    if (uShading == SHADING_GOURAUD) {
        // Already worked out per vertex.
        lit = albedo * (vGouraud * shadow + vault + uAmbient);
    } else if (uShading == SHADING_LAMBERT) {
        lit = albedo * (uLampColour * uLampIntensity * atten * diff + vault + uAmbient);
    } else {
        lit = albedo * (lamp + vault + uAmbient);
    }

    // Emissive surfaces glow on their own.
    lit += albedo * uEmissive;

    // Reinhard tone mapping, so bright spots do not clip to white.
    lit = lit / (lit + vec3(1.0));

    // Linear to sRGB. The only gamma correction in the project.
    lit = pow(lit, vec3(1.0 / 2.2));

    FragColor = vec4(lit, albedoSample.a);
}
