// Config.h - every tunable number in the project. Add constants here, not in room files.

#pragma once

namespace lumen::cfg {

// ---- world -----------------------------------------------------------------
// 1 unit = 1 metre. Plan +y (north) is world -Z. Up is +Y.
inline constexpr float kWallHeight      = 4.2f;   // m, top of the vertical wall
inline constexpr float kVaultApex       = 6.0f;   // m, top of the vault curve
inline constexpr float kWallThickness   = 0.45f;  // m
inline constexpr float kDoorHeight      = 2.6f;   // m

// ---- camera / movement -----------------------------------------------------
inline constexpr float kEyeHeight       = 1.65f;  // m
inline constexpr float kWalkSpeed       = 2.4f;   // m/s
inline constexpr float kRunSpeed        = 4.2f;   // m/s with shift held
inline constexpr float kMouseSens       = 0.085f; // degrees per pixel
inline constexpr float kPlayerRadius    = 0.32f;  // m, collision radius
inline constexpr float kNearPlane       = 0.05f;
inline constexpr float kFarPlane        = 160.0f;
inline constexpr float kFovDegrees      = 65.0f;

// ---- the carried lamp ------------------------------------------------------
inline constexpr float kLampInnerDeg    = 16.0f;  // full brightness inside this cone
inline constexpr float kLampOuterDeg    = 30.0f;  // zero by here
inline constexpr float kLampIntensity   = 6.5f;
inline constexpr float kLampRange       = 20.0f;  // m, light reaches zero here
inline constexpr float kLampLagHz       = 6.0f;   // lower = more lag behind the view
inline constexpr float kLampOffsetRight = 0.25f;  // m, held to one side
inline constexpr float kLampOffsetDown  = 0.18f;  // m, below eye level

// ---- room lighting ---------------------------------------------------------
// How bright a room gets when you walk in. 0 = lamp only, 1 = fully lit.
inline constexpr float kRevealLevel     = 0.30f;
inline constexpr float kRevealFadeIn    = 2.0f;   // seconds
inline constexpr float kRevealFadeOut   = 1.4f;   // seconds
inline constexpr float kRevealMargin    = 0.35f;  // m past the doorway before it triggers

inline constexpr float kVaultColour[3]  = {1.00f, 0.93f, 0.80f};
inline constexpr float kLampColour[3]   = {1.00f, 0.88f, 0.68f};

// ---- sculpture ----------------------------------------------------------------
// Soft light on the statues, as a share of the room reveal level, so it follows
// the lighting mode. The atrium is lit by daylight through the oculus; each
// niche has a hidden light above the statue.
inline constexpr float kOculusLight     = 0.85f;
inline constexpr float kNicheLight      = 0.45f;
inline constexpr float kOculusRim       = 0.5f;   // m, depth of the drum round the oculus
inline constexpr float kSkyGlow         = 1.1f;   // how bright the sky through the oculus is

// ---- the torch bearer ------------------------------------------------------------
// Who they are and which clips they play is in assets/intro.json.
inline constexpr int   kMaxJoints          = 100;   // must match MAX_JOINTS in lumen.vert
inline constexpr float kBearerPost[2]      = {1.5f, 1.2f};  // m west and south of the oculus centre
inline constexpr float kBearerStandOff     = 1.4f;  // m from the visitor when offering the torch
inline constexpr float kBearerTurnRate     = 200.0f;  // degrees per second
inline constexpr float kBearerLight        = 0.30f; // soft light on the bearer, as if from the torch
inline constexpr float kBearerPlinthHeight = 0.32f; // m, the step the statue stands on
inline constexpr float kBearerPlinthSize   = 0.85f; // m, square
inline constexpr float kBearerStepLength   = 0.7f;  // m of walking to step down off it, or up onto it
inline constexpr float kBearerRefreeze     = 1.2f;  // s back on the plinth before turning to stone
inline constexpr float kIntroStartDelay    = 1.5f;  // s before the bearer sets off
inline constexpr float kIntroPromptAfter   = 1.0f;  // s of offering before E works
inline constexpr float kIntroAutoTake      = 12.0f; // s, then the lamp is handed over anyway
inline constexpr float kIntroHandoverTime  = 1.2f;  // s for the light to move into your hand
inline constexpr float kIntroPause         = 0.6f;  // s the bearer waits before going back
inline constexpr float kTorchFlameGlow     = 3.2f;  // how bright the flame looks

// ---- shadows from the lamp -------------------------------------------------
// One depth render from the lamp's point of view each frame (a cone needs one).
inline constexpr int   kShadowMapSize    = 2048;  // pixels, square
inline constexpr float kShadowNear       = 0.1f;  // m
inline constexpr float kShadowBias       = 0.0006f;
inline constexpr float kShadowNormalPush = 0.03f; // m, moves the lookup off the surface

// ---- art hanging -----------------------------------------------------------
inline constexpr float kHangCentreHeight = 1.52f; // m to the centre of a work
inline constexpr float kFrameDepth       = 0.09f; // m, how far the frame stands out
inline constexpr float kFrameWidth       = 0.11f; // m
inline constexpr float kWorkOffset       = 0.04f; // m, canvas gap from the wall
inline constexpr float kReliefStrength   = 1.6f;  // strength of the normal map

// ---- minimap ---------------------------------------------------------------
inline constexpr int   kMinimapPx        = 260;   // square, pixels
inline constexpr float kMinimapMargin    = 18.0f; // px from the screen corner
inline constexpr float kMinimapScale     = 4.6f;  // px per metre
inline constexpr float kMinimapOpacity   = 0.82f;

// ---- lighting modes --------------------------------------------------------
// Cycled with L. Set kDefaultLightingMode to 1 for the final submission.
struct LightingPreset {
    const char* name;
    float reveal;     // how bright a room gets, 0 to 1
    float ambient;    // so nothing is pure black
    float lampScale;  // multiplies kLampIntensity
};

inline constexpr LightingPreset kLightingPresets[] = {
    {"Exhibition", 1.00f, 0.38f, 0.60f},  // everything visible
    {"Gallery",    0.30f, 0.012f, 1.00f}, // the intended look
    {"Lamp only",  0.00f, 0.004f, 1.20f}, // lamp alone
};
inline constexpr int kLightingPresetCount =
    static_cast<int>(sizeof(kLightingPresets) / sizeof(kLightingPresets[0]));

inline constexpr int kDefaultLightingMode = 0;   // 1 for the final submission

// ---- the algorithm demo ----------------------------------------------------
inline constexpr int kDemoW = 1280;   // fixed canvas size, scaled to the window
inline constexpr int kDemoH = 720;

// ---- shading modes ---------------------------------------------------------
// One shader. To add a mode, add a value here and a branch in shaders/lumen.frag.
enum class Shading : int {
    Lambert      = 0,  // diffuse only
    BlinnPhong   = 1,  // diffuse and specular per pixel, the default
    Gouraud      = 2,  // per vertex, used on purpose in Gallery Four
    NormalMapped = 3,  // Blinn-Phong with a bumped surface
    Emissive     = 4,  // glows on its own
    Triplanar    = 5,  // texture projected from three sides, for scans with no UVs
};

} // namespace lumen::cfg
