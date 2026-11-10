// Config.h - every tunable number in the project lives here.
//
// RULE: no magic numbers anywhere else. If you find yourself typing a constant
// into a room file or a shader uniform, add it here instead and reference it.
// This is the file we argue about in week 8 when we are tuning how the gallery
// feels, and it is the only file where that argument should produce a diff.

#pragma once

namespace lumen::cfg {

// ---- world -----------------------------------------------------------------
// One world unit is one metre. Plan +y (north) maps to world -Z. Up is +Y.
inline constexpr float kWallHeight      = 4.2f;   // m, top of the vertical wall
inline constexpr float kVaultApex       = 6.0f;   // m, top of the vault curve
inline constexpr float kWallThickness   = 0.45f;  // m
inline constexpr float kDoorHeight      = 2.6f;   // m

// ---- camera / movement -----------------------------------------------------
inline constexpr float kEyeHeight       = 1.65f;  // m
inline constexpr float kWalkSpeed       = 2.4f;   // m/s - gallery pace, not a shooter
inline constexpr float kRunSpeed        = 4.2f;   // m/s with shift held
inline constexpr float kMouseSens       = 0.085f; // degrees per pixel
inline constexpr float kPlayerRadius    = 0.32f;  // m, collision capsule radius
inline constexpr float kNearPlane       = 0.05f;
inline constexpr float kFarPlane        = 160.0f;
inline constexpr float kFovDegrees      = 65.0f;

// ---- the carried lamp ------------------------------------------------------
// A cone, not a bare bulb: one shadow pass instead of six. See docs/LIGHTING.md.
inline constexpr float kLampInnerDeg    = 16.0f;  // full brightness inside this cone
inline constexpr float kLampOuterDeg    = 30.0f;  // falls to zero by here
inline constexpr float kLampIntensity   = 3.2f;
inline constexpr float kLampRange       = 14.0f;  // m, where attenuation ~0
inline constexpr float kLampLagHz       = 6.0f;   // lower = lamp swings behind your view more
inline constexpr float kLampOffsetRight = 0.25f;  // m, lamp is held to one side
inline constexpr float kLampOffsetDown  = 0.18f;  // m, and a little below eye level

// ---- the per-room reveal ---------------------------------------------------
// THIS IS THE NUMBER THE TEAM ARGUED ABOUT. The gallery is unlit; crossing a
// threshold fades that room's vault slot up to kRevealLevel, never to 1.0.
//   0.00 = pure lamp. Most atmospheric, but you cannot see a whole painting.
//   0.30 = room reads as a space, composition visible, lamp still does real work.
//   1.00 = ordinary gallery lighting; the concept evaporates.
// Walk the build and change this one float before you change anything else.
inline constexpr float kRevealLevel     = 0.30f;
inline constexpr float kRevealFadeIn    = 2.0f;   // seconds
inline constexpr float kRevealFadeOut   = 1.4f;   // seconds
inline constexpr float kRevealMargin    = 0.35f;  // m past the threshold before it triggers

// Colour of the bounced vault light. Warm, because it is bouncing off concrete.
inline constexpr float kVaultColour[3]  = {1.00f, 0.93f, 0.80f};
inline constexpr float kLampColour[3]   = {1.00f, 0.88f, 0.68f};

// ---- art hanging -----------------------------------------------------------
inline constexpr float kHangCentreHeight = 1.52f; // m to the centre of a work
inline constexpr float kFrameDepth       = 0.09f; // m, how far the frame stands off
inline constexpr float kFrameWidth       = 0.11f; // m, moulding width
inline constexpr float kWorkOffset       = 0.04f; // m, canvas stands off the wall
inline constexpr float kReliefStrength   = 1.6f;  // fake impasto from the image itself

// ---- minimap ---------------------------------------------------------------
// Rasterised by hand with our own Bresenham / midpoint circle. See core/Raster.h.
inline constexpr int   kMinimapPx        = 260;   // square, pixels
inline constexpr float kMinimapMargin    = 18.0f; // px from the screen corner
inline constexpr float kMinimapScale     = 4.6f;  // px per metre
inline constexpr float kMinimapOpacity   = 0.82f;

// ---- shading modes ---------------------------------------------------------
// One shader, one enum. Do NOT write a second shader - add a mode here and a
// branch in shaders/lumen.frag, and say in your room why you chose it.
enum class Shading : int {
    Lambert      = 0,  // diffuse only, matte surfaces
    BlinnPhong   = 1,  // the default: diffuse + specular, per fragment
    Gouraud      = 2,  // per-vertex, deliberately: the artefact is the lesson
    NormalMapped = 3,  // fake relief, spectacular under a moving lamp
    Emissive     = 4,  // glows on its own; pair with bloom in the generated room
};

} // namespace lumen::cfg
