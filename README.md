# BrightBox3D

BrightBox3D is a 3D game engine with its own editor, written in C++20.

This repository contains:

- `Engine/`: the engine library (`libBrightBox3D`) and its shaders and fonts.
- `Editor/`: the editor application.
- `Player/`: the standalone runtime that packaged games run on.
- `Tools/`: the reflection code generator used by the build.
- `Tests/`: unit, rendering, editor and end-to-end tests.
- `Samples/ArenaDemo/`: a sample game project.

## Tech stack

| Area | Technology |
|---|---|
| Language | C++20 |
| Build | CMake 3.24+ (all libraries are fetched automatically); game code is built with Ninja, which is built alongside the editor |
| Graphics | Metal (macOS), DirectX 11 (Windows) |
| Shaders | HLSL, compiled with FXC on Windows and translated to Metal with glslang + SPIRV-Cross |
| Windowing and input | GLFW |
| Physics | Jolt Physics |
| Editor UI | Dear ImGui (docking) and ImGuizmo |
| Game UI | pugixml (markup), Yoga (flexbox layout), FreeType + msdf-atlas-gen (text) |
| Assets | RapidJSON, stb_image, ufbx (FBX models) |
| Anti-aliasing | SMAA reference implementation, FXAA, TAA |
| Icons and fonts | Lucide, Roboto |
| Tests | GoogleTest |

## Prerequisites

- CMake 3.24 or newer.
- A C++20 compiler:
  - macOS: Apple Clang (Xcode or the Command Line Tools).
  - Windows: MSVC 2022. The Windows build has not been tested yet.
- Git and an internet connection for the first configure, which downloads the third-party libraries.

## Building

From the repository root:

```
cmake -S . -B build
cmake --build build -j
```

Everything is built into `build/bin/`. The first configure takes a few minutes while dependencies download and build.

## Running the editor

Open the sample project:

```
./build/bin/BrightEditor Samples/ArenaDemo
```

Or start the editor without arguments to get the project launcher, where you can create a new project or open a recent one:

```
./build/bin/BrightEditor
```

Run these commands from the repository root, since the paths are relative.

## Running tests

```
./build/bin/BrightTests
```

The end-to-end tests compile, package and run the sample game, so they need CMake on your `PATH`.
