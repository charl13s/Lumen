# Lighting in LUMEN

The one-sentence version, which should go in the presentation verbatim:

> **The gallery is unlit, and the visitor is the only cause of light in it —
> directly in the corridor, through the lamp they carry, and by their presence in
> the rooms.**

---

## Why there are two lights

A lamp held up to a painting gives you a bright ellipse and dark corners. It is
atmospheric, and it defeats the thing a gallery is for: you never see a whole
work at once. Real galleries work hard to avoid exactly that.

So LUMEN has two sources, and they do different jobs.

| | The lamp | The vault |
|---|---|---|
| Where | the visitor's hand | the slot above each room |
| Quality | hard, raking, moves with you | soft, from above, already bounced |
| Falls off | fast — inverse square | barely, across the room |
| Shows you | a painting's **surface** — brushwork, varnish sheen | a painting's **composition** |
| When | always | only while you are in the room |

Walk into a room and the vault slot fades up over two seconds; the composition
resolves. Walk up to a work and the lamp rakes across it; the brushwork appears.
That is close to how people behave in real galleries — see it from a distance,
then lean in.

## The reveal is a dial, not a switch

`cfg::kRevealLevel` in `src/core/Config.h`:

| Value | What it feels like |
|---|---|
| `0.00` | pure lamp. Maximum atmosphere; you cannot see a whole painting |
| `0.30` | **the current setting.** The room reads as a space, composition is visible, the lamp still does real work |
| `1.00` | ordinary gallery lighting. The concept evaporates |

It is one float. Build it, walk it, and argue about the number — do not argue
about it in the abstract.

The fade is deliberately asymmetric: in over `kRevealFadeIn` (2.0 s), out over
`kRevealFadeOut` (1.4 s). The room wakes slowly and lets go promptly behind you.

`kRevealMargin` pulls the threshold test inward so the light does not strobe
while someone stands on the sill.

## Why the lamp is a cone and not a bulb

This is the single most consequential decision in the lighting, and it is worth
being able to explain it.

Shadows are faked with **shadow mapping**: render the scene once from where the
light is, store how far away the nearest surface was in each direction, then when
shading a point ask whether it is further from the light than what the light saw.
If it is, something is blocking it — draw it in shadow.

But rendering always happens through a camera, and a camera points one way.

- A **bare bulb** shines in every direction, so one render cannot capture what it
  sees. You need six — up, down, left, right, forward, back — stitched into a cube
  map. Six times the cost every frame, plus the logic to pick a face, plus seams.
- A **cone** is exactly the shape a camera already sees. One render.

Same visual quality, roughly a sixth of the work. Shadows are the feature most
likely to be cut under deadline pressure, and this makes them about six times
less likely to be cut. A hooded lamp also suits the piece: it is directional, so
the visitor has to *aim* it at the art rather than floating around glowing.

Implemented as `uLampInnerCos` / `uLampOuterCos` in `shaders/lumen.frag`.

## Attenuation

By default an OpenGL light does not weaken with distance — a wall two metres away
and one thirty metres away come out identically bright. In a dark gallery that
reads as "the lights are on," not "I am holding a lamp."

```glsl
float a = 1.0 / (1.0 + 0.22 * dist + 0.20 * dist * dist);
float edge = clamp(1.0 - (dist / range) * (dist / range), 0.0, 1.0);
return a * edge * edge;
```

Inverse-square, with a smooth cut to zero at `kLampRange` so there is no visible
boundary where the light stops. **This is the line that turns a light source into
a carried light source.** Remove it and the whole concept fails.

## The lamp lags

`Lamp::follow` smooths the lamp's direction toward the view direction at
`kLampLagHz`. A lamp locked rigidly to the view looks robotic; a slight
rotational lag reads as a hand holding something. The smoothing is exponential
and frame-rate independent, so it feels the same at 30 fps and 144 fps.

## The shading modes

One shader, `shaders/lumen.frag`, with a mode enum. **Do not write a second
shader** — add a value to `cfg::Shading` and a branch here.

| Mode | What it does | Where it is used |
|---|---|---|
| `Lambert` | diffuse only. Matte: chalk, plaster, concrete | vaults, labels, Gallery Five |
| `BlinnPhong` | diffuse + specular, per fragment. **The default** | walls, frames, Gallery One and Two |
| `Gouraud` | lighting at the vertices, interpolated across the triangle | Gallery Four, deliberately |
| `NormalMapped` | Blinn-Phong with the normal perturbed per pixel | every painting canvas |
| `Emissive` | glows on its own | Gallery Three's generated work |

### Why Gouraud is in Gallery Four on purpose

Per-vertex lighting evaluates at the three corners of a triangle and interpolates
between them. On a large wall quad with a small moving highlight, the highlight
can land in the middle of a triangle where no corner ever sees it — so it dims,
flickers, or vanishes as you walk past.

That is a real, visible, explainable artefact, and it is directly on the syllabus.
Side by side with the Blinn-Phong rooms it is a demonstration rather than a bug.
It is labelled on the wall text and explained in that room's `techniqueNotes()`.

### Normal mapping without a normal map

`TextureCache::reliefFrom` runs a Sobel pass over each painting's own luminance
and treats the gradient as surface slope. Fake impasto, derived from the image
itself: no extra asset, about fifteen lines, and it is most visible exactly when
the carried lamp rakes across at a grazing angle.

It is the cheapest large improvement available to us. A painting without it reads
as a sticker on a wall.

## Gamma — read this before you touch anything

Colour textures are uploaded as `GL_SRGB8` / `GL_SRGB8_ALPHA8` so the GPU
linearises them on read. All lighting maths happens in linear space. The final
line of the fragment shader converts back:

```glsl
lit = pow(lit, vec3(1.0 / 2.2));
```

**There is exactly one gamma correction in the project and that is it.** Add a
second anywhere and every light intensity has to be re-tuned from scratch.

Normal maps and other data textures are uploaded as `GL_RGB8` — linear — because
they are not colours.

## Still to do

- [ ] shadow mapping from the lamp cone (one depth pass) — *first thing to cut
      if week 10 is tight*
- [ ] a bloom pass for Gallery Three's emissive work
- [ ] baked vertex ambient occlusion in the corners, so the rooms stop looking
      moulded from plastic
