# The artwork

## Where images come from

Museum open-access collections only. Every image must be public domain, with its
credit line recorded in `assets/works.json` and rendered on the wall label.

The course outline treats plagiarism seriously, and "we found it on Google
Images" is the wrong answer when someone asks. Using a museum's own open-access
API with the public-domain flag is both correct and easy to defend.

### Art Institute of Chicago — start here

No API key, no account. A REST endpoint and an IIIF image server.

```
# search, public domain only
https://api.artic.edu/api/v1/artworks/search?q=still+life&query[term][is_public_domain]=true

# one artwork, with the fields you need
https://api.artic.edu/api/v1/artworks/27992?fields=id,title,artist_display,date_display,credit_line,image_id,is_public_domain

# the image, at whatever size you ask for
https://www.artic.edu/iiif/2/{image_id}/full/2048,/0/default.jpg
```

The IIIF server is the useful part: you request 2048px directly rather than
downloading a huge file and resizing. Over 50,000 public-domain images.

Docs: <https://api.artic.edu/docs>

### Rijksmuseum

Over 800,000 objects, free API key. The best collection in the world for Dutch
Golden Age painting — and Rembrandt and Vermeer are painters *about* light, which
is on theme for us.

<https://www.rijksmuseum.nl/api/>

### The Met

Public-domain works released under CC0, no key.

<https://collectionapi.metmuseum.org/public/collection/v1/objects>

### Rules

- 2048px minimum. The visitor can walk right up to a work.
- Record `credit`, `title`, `artist`, `date` in `works.json` as the museum states
  them. Do not paraphrase a credit line.
- Check the public-domain flag per object. Not everything in a collection is
  public domain, even at a museum with an open-access programme.

## Making a painting not look like a sticker

In order of payoff:

1. **Relief from the image itself.** `TextureCache::reliefFrom` derives a normal
   map from the painting's own luminance. Brushwork catches the lamp. No extra
   asset. This is the biggest single improvement available.
2. **Two materials.** Diffuse for the paint, a weak broad specular for the
   varnish. Real paintings have a faint sheen that shifts as you move.
3. **Frames as extruded profiles.** Never model the ornament. A gilt baroque
   frame is tens of thousands of triangles modelled honestly; extrude a simple
   moulding and put the ornament in a normal map. `build::buildFramedWork` does
   this already.
4. **Anisotropic filtering.** In a loop corridor you see works at grazing angles
   constantly. Handled in `Gfx.cpp`.

## Texture budget

Thirty-five works at 2048² uncompressed with mipmaps is well over half a
gigabyte. With block compression it is around 100 MB.

Decide this in Phase 2, not when the machine starts thrashing in week 11. If
memory becomes a problem before compression is in: drop distant works to 1024.

## Gallery Three — the generated room

The requirement is "at least one room of generated artwork." Treated as a
technical box to tick, it reads as filler. There is a better framing available:

**Make it a room of algorithmic art, and put our own work in that lineage.**

Computer-generated art has a real history and it starts almost exactly where we
are. Georg Nees and Frieder Nake exhibited plotter drawings made from algorithms
in Stuttgart in 1965. Vera Molnár spent decades making work from systematic rules
with controlled randomness — grids of squares that drift progressively out of
alignment, which is beautiful and is about twenty lines of code. Manfred Mohr
worked through the geometry of the hypercube. Harold Cohen spent forty years
building AARON, a program that drew.

So the room gets a wall text explaining that lineage, a few homage pieces
generated in those traditions, and then ours. That is curation rather than
filler, and it is what justifies the whole project existing as a *computer
graphics* exercise rather than a 3D modelling one.

### What to generate, in rising order of effort

| | Idea | Effort |
|---|---|---|
| 1 | **Molnár-style perturbed grids.** A grid of squares; perturb each vertex by an amount that grows across the canvas. | Trivial — already implemented in `tools/make-placeholder-assets.py`, port it to the room |
| 2 | **Reaction-diffusion.** Gray-Scott as a ping-pong fragment shader. Organic, mesmerising, and a live GPU computation rather than a loaded image. | Moderate |
| 3 | **A procedural sculpture.** An L-system or parametric surface as actual geometry, in a corridor niche where the lamp rakes across it. | Higher — cut this first if week 10 is tight |

### Generate from a seed at runtime

Seed from the clock, generate the room's works on load, and display the seed on
the wall label. Every visit is a different exhibition. Nobody who walks through
twice sees the same show.

For a project about a gallery you cannot physically visit, that is a real
conceptual payoff, and it costs almost nothing — the alternative was generating
once and shipping a PNG anyway.

## Flat or sculptural

Both, and the lighting decides where each goes.

Under soft bounced vault light, flat paintings read beautifully and sculpture
goes slightly flat — soft light from above is exactly what sculpture does not
want. Sculpture wants hard raking light.

So: **paintings in the rooms, sculpture in the corridor niches**, lit only by the
passing lamp. Both lighting models then have a job, the corridor stops being dead
space, and we have a reason to have built both textured quads and real meshes.

The plan has four niches, at the points where there is solid wall between the
room doors.
