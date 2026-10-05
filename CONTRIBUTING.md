# Contributing to LUMEN

Five people, one build, thirteen weeks. These rules exist so that nobody's work
blocks anybody else's, and so that each of us can point at our own contribution
at the end.

---

## Who owns what

| Member | Room | Also responsible for |
|---|---|---|
| Shawn Kuria | Gallery One | integration, the repository, `src/main.cpp` |
| Charles Murage | Gallery Two | camera, movement, collision |
| Timothy Kimani | Gallery Three | lighting and shading, the generated artwork |
| Erykah Diang'a | Gallery Four | geometry, the floor plan and its generator |
| Priscillah Njoroge | Gallery Five | testing, documentation |

"Responsible for" means you are the first reviewer on changes in that area and
the person who explains it in the viva. It does not mean you are the only one
allowed to understand it — the brief asks every member to understand the whole
application, so rotate pairs occasionally and do not let any one area have
exactly one person who knows it.

## The one rule that matters

> **A room file may not call OpenGL.**

Not `glGenTextures`, not `glDrawArrays`, not `glUniform`. Your room *describes*
what it wants; `src/core/` draws it. The full interface is in
[`src/core/Room.h`](src/core/Room.h) — read it before you start.

If five of us each write a texture loader we get five loaders, five copies of the
sRGB mistake, and a merge conflict every week. With one loader there is one place
to fix it, and when Timothy improves the lighting, every room improves for free.

## Where things go

| You want to… | Do this |
|---|---|
| hang a work, place a prop, animate something | edit **your** room file only |
| add a constant | `src/core/Config.h` — never a literal in a room file |
| load a texture | `ctx.texture()` / `ctx.relief()`, so it is cached and the gamma is right |
| add a new shading mode | a value in `cfg::Shading` **and** a branch in `lumen.frag` — never a second shader |
| change the floor plan | `tools/generate-floor-plan.py`, then rerun it |
| change anything in `src/core/` | **open an issue first**, then a PR |

`src/core/` is shared. A change there affects five people, so it goes through a
pull request with a reviewer, even when it is one line.

## Git

```bash
git checkout -b room/charles          # your branch, named for you
# ... work ...
git add src/rooms/RoomTwo.cpp
git commit -m "Gallery Two: hang the first three works"
git push -u origin room/charles
```

Then open a pull request and ask one teammate to approve it.

**Rules:**

1. **Never push to `main` directly.** Branch, PR, one approval, merge.
2. **`main` always builds.** If you merge something that breaks it, fixing it is
   the most urgent thing you have.
3. **Commit under your own account.** Section 4.1 of the brief says GitHub
   activity is the evidence of individual contribution. Check
   `git config user.email` matches your GitHub account before your first commit.
4. **Commit regularly, not in bulk.** A single enormous commit in week 12 looks
   exactly like what the brief warns against. Small weekly commits are the
   evidence; they are also easier to review.
5. **Run the tests before you push:** `build\Debug\lumen_tests.exe`.

Because each room is a single file that only its owner touches, merge conflicts
should be close to zero. If you are getting conflicts, something has leaked out
of a room file into shared code — say so rather than resolving it quietly.

## Weekly integration

Same hour, every week, all five present. Everyone pulls `main`, builds, and walks
the gallery together. Divergence found in week 4 costs an hour; the same
divergence found in week 11 costs the project.

Bring to it: what you merged, what you are blocked on, and anything you want
moved into `core/`.

## Definition of done, per room

A room is finished when all of these are true:

- [ ] it builds on a clean clone, with no warnings
- [ ] it has works hanging at every hang point the plan gives it
- [ ] it has at least one object with real depth (plinth, sculpture, bench)
- [ ] every texture is public domain with its credit line in `assets/works.json`
- [ ] `techniqueNotes()` is written and explains *why* each technique was chosen
- [ ] it looks right at `kRevealLevel` both at `0.0` and at `0.30`
- [ ] you can walk in and out of it without getting stuck on the doorway
- [ ] someone else has walked it and said so

## Code style

Keep it boring and readable. We are all coming to C++ from Java, and clever code
costs more than it saves here.

- `PascalCase` types, `camelCase` functions and variables, `kThing` constants
- `member_` trailing underscore for private members
- include your own header first, then standard library, then third-party
- `const` by default; pass big things by `const&`
- comment *why*, not *what* — the code already says what

## If you are stuck

Say so in the group, that day. The single most expensive thing on a thirteen-week
project is somebody quietly stuck for a week. Nobody here minds a question;
everybody minds a surprise in week 12.
