# Billiards Sim

A 2D billiards simulation in C++ and OpenGL, built one stage at a time. Balls
move, bounce off the walls, and collide with each other. The physics is written
by hand: no physics library.

The project started from an OpenGL boilerplate (GLFW and GLAD vendored, window
and input handling in `src/core/`). Each commit after the boilerplate adds one
stage of the simulation, so the git history doubles as a learning history.

## Prerequisites

- **MinGW-w64 via MSYS2**. Install from [msys2.org](https://www.msys2.org/),
  then install the `mingw-w64-x86_64-gcc` toolchain (or equivalent) from
  within the MSYS2 shell.

## Setup

1. Clone this repo.
2. Open `.vscode/tasks.json` and edit the `"command"` field to point at your
   own `g++.exe` (e.g. `C:/msys64/mingw64/bin/g++.exe`).
3. Run the `Build Main` task, then run `main.exe`.
4. Two balls bounce around a green window. `Esc` closes it.
5. Optionally run the `Build Triangle Example` task and `triangle.exe` to
   see an orange triangle.

The project builds as C++20 (`-std=c++20`).

## How the simulation works

The window is square (600x600) and the play area is the full screen, from -1 to
1 on both axes. The square window keeps the balls circular.

Every frame, `src/main.cpp` does the following:

1. Measures `deltaTime`, the time since the previous frame.
2. For each ball, moves it by `velocity * deltaTime` (`updateBall`) and keeps
   it inside the walls (`resolveWallCollision`).
3. Checks every pair of balls once (`checkBallCollision`). Touching pairs are
   resolved (`resolveBallCollision`).
4. Draws every ball.

Resolving a ball-ball collision takes three steps, in order:

1. `computeCollisionNormal`: the unit direction from one ball's center to the
   other's.
2. `exchangeNormalVelocity`: the two balls swap their velocity along that
   direction. Their sideways motion is unchanged. This is the equal-mass,
   perfectly elastic case.
3. `correctPenetration`: pushes the balls apart along the same direction so
   they stop overlapping.

## Stages

Each stage is one commit, tagged so you can jump to it:

| Stage | Tag | What it adds |
|---|---|---|
| 1 | *(Initial commit)* | The boilerplate: window, clear color, `Esc` to close |
| 2 | `stage-2` | `Ball`, circle mesh, shaders, `drawBall`: one static ball |
| 3 | `stage-3` | `updateBall` and frame timing: the ball drifts off screen |
| 4 | `stage-4` | `resolveWallCollision`: the ball bounces off all four walls |
| 5 | `stage-5` | Two balls, each bouncing independently, passing through each other |
| 6 | `stage-6` | `checkBallCollision`: detects contact and prints it |
| 7 | `stage-7` | Ball-ball collision response: the balls bounce off each other |

To see the code as it was at a stage, run `git checkout stage-4` (or any tag),
then `git checkout main` to come back.

## Project structure

```
src/
  main.cpp                the simulation
  glad.c                  vendored GLAD loader
  core/
    glfw_context.h/.cpp   gfx::GlfwContext, initializes and terminates GLFW
    window.h/.cpp         gfx::Window, creates the window and loads GLAD
    input.h/.cpp          gfx::processInput, keyboard handling
  examples/
    triangle.cpp          triangle rendered on top of core/
```

Everything in `core/` lives in the `gfx` namespace and uses RAII: an object
sets up its resource in its constructor and releases it in its destructor, so
cleanup happens on every exit path. Startup failures throw
`std::runtime_error`, which `main` catches and prints.

## Vendored dependencies

`include/glad`, `include/GLFW`, `include/KHR`, `lib/libglfw3dll.a`, and the
root `glfw3.dll` are already included in this repo. You shouldn't need to
reinstall anything, but here's where they came from in case a dependency
ever needs to be regenerated or replaced:

- **GLAD**, generated at [glad.dav1d.de](https://glad.dav1d.de/) with:
  - Language: `C/C++`
  - API: `gl` -> Version `3.3`
  - Profile: `Core`
  - All other APIs: `None`
  - "Omit KHR": unchecked
- **GLFW**, downloaded from the
  [GLFW downloads page](https://www.glfw.org/download.html). Use the Windows
  precompiled binaries, and pull the files from the `lib-mingw-w64` folder
  specifically.

## PATH requirement

Besides pointing `tasks.json` at your `g++.exe`, you also need to add your
compiler's `bin` folder (e.g. `C:\msys64\mingw64\bin`) to your system `PATH`
environment variable.

This is separate from the `tasks.json` compiler path. The `tasks.json`
`"command"` only tells VS Code/Cursor which executable to launch directly.
It doesn't help the processes that compiler spawns internally (such as
`cc1plus.exe`) find their own DLL dependencies. Those child processes
resolve their DLLs through the system PATH, so without this step the build
can fail even though `tasks.json` looks correctly configured.

## Troubleshooting

- **Build fails silently, with no error text at all.** This is almost
  always the missing PATH entry above. Add the compiler's `bin` folder to
  your system PATH and try again.
- **Program compiles but won't run, or fails to launch immediately.**
  `libglfw3dll.a` is an import library: it satisfies the linker, but the
  actual GLFW code lives in `glfw3.dll` at runtime. That DLL has to sit next
  to the built `.exe` (it already does, in the repo root), or the program
  won't start even though it compiled fine.
