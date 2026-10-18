# Billiards Sim

A 2D billiards game in C++ and OpenGL. The physics is written by hand, with no
physics library. The earlier experiments (bouncing balls, a spring-based soft
body, a Verlet ragdoll) are kept as standalone programs in `src/examples/`.

<div align=center>
<img width="800" alt="image" src="https://github.com/user-attachments/assets/1fe8c8e0-18cd-4c5b-8464-b8ca5fd8b253" />
</div>

GLFW and GLAD are vendored, and window and input handling live in `src/core/`.
Each commit after the boilerplate adds one stage, so the git history shows how
the project was built.

## Prerequisites

- **MinGW-w64 via MSYS2**. Install from [msys2.org](https://www.msys2.org/),
  then install the `mingw-w64-x86_64-gcc` toolchain from the MSYS2 shell.

## Setup

1. Clone this repo.
2. Open `.vscode/tasks.json` and set the `"command"` field of both tasks to your
   `g++.exe` (e.g. `C:/msys64/mingw64/bin/g++.exe`).
3. Run the `Build Main` task, then run `main.exe`.
4. To build an example, run the `Build Example` task and pick a name. It
   produces `<name>.exe` in the repo root.

The project builds as C++20.

## Controls

- **Aim:** move the mouse. The stick points at the cursor.
- **Shoot:** hold the left mouse button, drag back to pull the stick, release to
  hit. A longer pull is a harder shot.
- `Esc` closes the window.

The stick appears only when all balls have stopped. Balls that fall into a
pocket are removed. If the cue ball is pocketed, it returns to the head spot once
the table is still.

## How it works

All sizes are in meters. The window is sized to fit the table and its rail.

The simulation runs at a fixed 480 Hz, driven by a time accumulator. Each step
(`stepBalls`):

1. Moves each ball by `velocity * dt`.
2. Slows it with a constant rolling deceleration (`applyFriction`). Below
   `kStopSpeed` the ball stops.
3. Resolves the cushions (`resolveCushions`). Each cushion is a segment, and its
   ends are the jaw tips at the pockets. A ball bounces off a segment or a tip
   with `kCushionRestitution`. A ball whose center enters a pocket circle is
   removed.
4. Resolves ball-ball collisions (`resolveBallCollision`). Touching, approaching
   balls exchange velocity along the line between their centers, scaled by
   `kBallRestitution`, and are pushed apart.

The stick is handled in `updateCue`. While balls move, input is ignored. At rest,
the aim follows the mouse. Holding the left button and dragging back sets the
pull, up to `kMaxPull`. On release, the cue ball gets a speed of
`pull / kMaxPull * kMaxShotSpeed` along the aim direction.

Everything is drawn each frame from 2D vertex lists through one dynamic buffer,
back to front: rail, cloth, cushions, pockets, diamonds, balls, stick.

The values to tune are the constants at the top of `src/main.cpp`:
`kRollingResistance`, `kCushionRestitution`, `kBallRestitution`,
`kMaxShotSpeed`, `kMaxPull`, `kBallScale` and `kPocketRadius`.

## Examples

Each example is a standalone program in `src/examples/<name>/main.cpp`, taken
from the stage where it was finished.

| Example | What it shows | From |
|---|---|---|
| `triangle` | An orange triangle on top of `core/` | boilerplate |
| `ball_collisions` | Two balls bouncing off the walls and each other | `stage-7` |
| `bouncing_ball` | Gravity and per-ball restitution | `stage-11` |
| `mass_spring_square` | Four points and six damped springs forming a soft square | `stage-16` |
| `ragdoll` | A Verlet stick figure you can drag with the mouse | `stage-23` |

**Bouncing ball.** Gravity adds a downward acceleration to each ball every
frame. Each ball has its own `restitution`, the fraction of speed kept after a
floor bounce, so two balls dropped together settle differently.

**Mass-spring square.** Each spring pulls its two points toward a rest length in
proportion to its stretch, and a damping term slows the motion. The four edge
springs let the square fold flat, and the two diagonal springs hold its shape.

**Ragdoll.** The figure is 11 points joined by 10 sticks. A point stores its
current and previous position, and their difference is its velocity (Verlet
integration). A stick moves its two points back to a fixed distance, and the
sticks are applied `kIterations` times per frame so the body stays rigid. A
pinned point never moves. The floor is a position clamp. Holding the left mouse
button drags the nearest point.

## Stages

Stages 1 to 23 are tagged. The billiards game is the latest commits on `main`.

| Stage | Tag | What it adds |
|---|---|---|
| 1 | *(Initial commit)* | The boilerplate: window, clear color, `Esc` to close |
| 2 | `stage-2` | `Ball`, circle mesh, shaders, `drawBall`: one static ball |
| 3 | `stage-3` | `updateBall` and frame timing: the ball drifts off screen |
| 4 | `stage-4` | `resolveWallCollision`: the ball bounces off all four walls |
| 5 | `stage-5` | Two balls, each bouncing independently, passing through each other |
| 6 | `stage-6` | `checkBallCollision`: detects contact and prints it |
| 7 | `stage-7` | Ball-ball collision response: the balls bounce off each other |
| 8 | `stage-8` | Removes ball-vs-ball collision to isolate gravity work |
| 9 | `stage-9` | `applyGravity`: gravity accumulates into vertical velocity every frame |
| 10 | `stage-10` | Restitution: floor bounces lose energy, so the ball settles |
| 11 | `stage-11` | Per-ball restitution: two balls, two different behaviors |
| 12 | `stage-12` | `PointMass`, gravity and floor collision, drawn through dynamic buffers |
| 13 | `stage-13` | `Spring`: Hooke's Law pulls two point masses toward a rest length |
| 14 | `stage-14` | Damping: the spring loses energy and settles |
| 15 | `stage-15` | Four point masses and four edge springs: a square that folds flat |
| 16 | `stage-16` | Two diagonal springs: the square keeps its shape |
| 17 | `stage-17` | Verlet integration: a point falls with no stored velocity |
| 18 | `stage-18` | `Stick`: two points keep a fixed distance |
| 19 | `stage-19` | Pinned points: a chain hangs from a fixed point |
| 20 | `stage-20` | Relaxation: several passes per frame keep the sticks rigid |
| 21 | `stage-21` | The ragdoll: eleven points and ten sticks, hanging by the head |
| 22 | `stage-22` | Floor clamp: the ragdoll falls and collapses |
| 23 | `stage-23` | Mouse dragging: pick up and throw the ragdoll |
| 24 | *(on `main`, untagged)* | Examples moved to `src/examples/`; `main.cpp` becomes the billiards game |

To see the code at a stage, run `git checkout stage-4` (or any tag), then
`git checkout main` to come back.

## Project structure

```
src/
  main.cpp                the billiards game
  glad.c                  vendored GLAD loader
  core/
    glfw_context.h/.cpp   gfx::GlfwContext, initializes and terminates GLFW
    window.h/.cpp         gfx::Window, creates the window and loads GLAD
    input.h/.cpp          gfx::processInput, keyboard handling
  examples/
    triangle/             triangle rendered on top of core/
    ball_collisions/      two balls, walls and ball-ball collisions
    bouncing_ball/        gravity and restitution
    mass_spring_square/   soft body made of springs
    ragdoll/              Verlet stick figure
```

Everything in `core/` lives in the `gfx` namespace and uses RAII: an object
acquires its resource in its constructor and releases it in its destructor.
Startup failures throw `std::runtime_error`, which `main` catches and prints.

## Vendored dependencies

`include/glad`, `include/GLFW`, `include/KHR`, `lib/libglfw3dll.a`, and the
root `glfw3.dll` are included in this repo. If you need to regenerate or replace
them:

- **GLAD**, generated at [glad.dav1d.de](https://glad.dav1d.de/) with:
  - Language: `C/C++`
  - API: `gl` -> Version `3.3`
  - Profile: `Core`
  - All other APIs: `None`
  - "Omit KHR": unchecked
- **GLFW**, from the [GLFW downloads page](https://www.glfw.org/download.html).
  Use the Windows precompiled binaries and take the files from the
  `lib-mingw-w64` folder.

## PATH requirement

Besides pointing `tasks.json` at your `g++.exe`, add your compiler's `bin` folder
(e.g. `C:\msys64\mingw64\bin`) to your system `PATH`.

The `tasks.json` `"command"` only tells VS Code/Cursor which executable to
launch. The compiler's own child processes (such as `cc1plus.exe`) find their
DLLs through the system `PATH`, so without this entry the build can fail even
when `tasks.json` is correct.

## Troubleshooting

- **Build fails silently, with no error text.** Usually the missing `PATH` entry
  above.
- **Program compiles but won't run.** `libglfw3dll.a` only satisfies the linker.
  The GLFW code lives in `glfw3.dll`, which must sit next to the built `.exe`
  (it does, in the repo root).
