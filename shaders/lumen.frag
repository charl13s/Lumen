#version 330 core

// LUMEN - the one fragment shader.
//
// TWO LIGHTS, AND THEY DO DIFFERENT JOBS:
//
//   The lamp   a cone the visitor carries. Hard, raking, attenuating fast. It is
//              what shows you a painting's SURFACE when you walk up to it - the
//              brushwork in the relief map, the sheen on the varnish.
//
//   The vault  a soft wash from the slot above, bounced off the curved ceiling
//              in the manner of Kahn's Kimbell. It fades up when the visitor
//              enters the room and never reaches full brightness (see
//              cfg::kRevealLevel). It is what shows you a painting's
//              COMPOSITION from across the room.
//
// Nothing is lit when nobody is there. uVaultLevel is 0 in every room the
// visitor is not standing in, and the corridor has no vault at all.
//
// GAMMA: textures were uploaded as sRGB so the GPU linearised them on read. All
// of the maths below is in linear space, and the last line converts back for
// display. Do not add a second gamma correction anywhere.

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
uniform int   uShading;

uniform vec3  uLampPos;
uniform vec3  uLampDir;
uniform vec3  uLampColour;
uniform float uLampIntensity;
uniform float uLampRange;
uniform float uLampInnerCos;
uniform float uLampOuterCos;

uniform vec3  uVaultColour;
uniform float uVaultLevel;      // 0 .. cfg::kRevealLevel, never 1.0
uniform vec3  uViewPos;

uniform float uAmbient;         // a whisper, so pure black is not pure black

const int SHADING_LAMBERT       = 0;
const int SHADING_BLINN_PHONG   = 1;
const int SHADING_GOURAUD       = 2;
const int SHADING_NORMAL_MAPPED = 3;
const int SHADING_EMISSIVE      = 4;

float attenuation(float dist, float range) {
    float a = 1.0 / (1.0 + 0.22 * dist + 0.20 * dist * dist);
    float edge = clamp(1.0 - (dist / range) * (dist / range), 0.0, 1.0);
    return a * edge * edge;
}

void main() {
    vec4 albedoSample = texture(uAlbedo, vUV);
    vec3 albedo = albedoSample.rgb * uTint;

    // Surface normal. Normal mapping is what makes a flat painting catch the
    // lamp as you walk past - the relief is derived from the painting's own
    // luminance, so it costs no extra asset.
    vec3 N = normalize(vNormal);
    if (uShading == SHADING_NORMAL_MAPPED) {
        vec3 sampled = texture(uNormalMap, vUV).rgb * 2.0 - 1.0;
        N = normalize(vTBN * sampled);
    }

    vec3 V = normalize(uViewPos - vWorldPos);

    // ---- the carried lamp --------------------------------------------------
    vec3 toLight = uLampPos - vWorldPos;
    float dist   = length(toLight);
    vec3  L      = toLight / max(dist, 0.0001);

    // Cone falloff. A cone, not a sphere: one shadow pass instead of six.
    float theta = dot(-L, normalize(uLampDir));
    float cone  = clamp((theta - uLampOuterCos) /
                        max(uLampInnerCos - uLampOuterCos, 0.0001), 0.0, 1.0);
    cone = cone * cone;      // soften the cone edge

    float atten = attenuation(dist, uLampRange) * cone;

    float diff = max(dot(N, L), 0.0);
    vec3  lamp = uLampColour * uLampIntensity * atten * diff;

    if (uShading == SHADING_BLINN_PHONG || uShading == SHADING_NORMAL_MAPPED) {
        // Blinn-Phong: the halfway vector instead of the reflection vector.
        // Cheaper, and better behaved at grazing angles, which is most of what a
        // raking lamp produces.
        vec3  H    = normalize(L + V);
        float spec = pow(max(dot(N, H), 0.0), uShininess);
        lamp += uLampColour * uLampIntensity * atten * spec * uSpecular;
    }

    // ---- the vault wash ----------------------------------------------------
    // Light arriving from the slot above, already bounced, so it is broad and
    // comes mostly from straight up. Modelled as a hemisphere term rather than a
    // point light, because that is what a bounced source actually looks like.
    float sky   = 0.5 + 0.5 * dot(N, vec3(0.0, 1.0, 0.0));
    vec3  vault = uVaultColour * uVaultLevel * sky;

    // A faint wrap so vertical surfaces under the vault are not black.
    vault += uVaultColour * uVaultLevel * 0.25;

    // ---- combine -----------------------------------------------------------
    vec3 lit;
    if (uShading == SHADING_GOURAUD) {
        // Interpolated from the vertices. Compare the highlight on a wall here
        // against the same wall in Blinn-Phong: this is the artefact we are
        // demonstrating, not a bug.
        lit = albedo * (vGouraud + vault + uAmbient);
    } else if (uShading == SHADING_LAMBERT) {
        lit = albedo * (uLampColour * uLampIntensity * atten * diff + vault + uAmbient);
    } else {
        lit = albedo * (lamp + vault + uAmbient);
    }

    // Emissive surfaces glow on their own - the generated room's work gives light
    // back rather than only receiving it.
    lit += albedo * uEmissive;

    // Reinhard tone map, so a bright lamp close to a wall rolls off instead of
    // clipping to white.
    lit = lit / (lit + vec3(1.0));

    // Linear -> sRGB for display. The only gamma correction in the project.
    lit = pow(lit, vec3(1.0 / 2.2));

    FragColor = vec4(lit, albedoSample.a);
}
