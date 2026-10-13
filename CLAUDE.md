# BrightBox3D

C++20 3D game engine: an Unreal-style gameplay framework, a Unity-style editor, and C++ hot reload. The authoritative design is `docs/SPEC.md`; §17 there records implementation decisions. Feature specs for follow-up work live in `docs/specs/`. All specs are private: `docs/SPEC.md` and `docs/specs/` are excluded locally via `.git/info/exclude` and must never be added, committed or pushed. The README only describes the engine, its tech stack and how to build and launch it; it never mentions other engines. Game development guides belong in `docs/instructions/` (not written yet).

## Repository layout

| Path | Contents |
|---|---|
| `Engine/Source/<Module>` | Engine shared library `libBrightBox3D`. Modules: `Core`, `Math`, `Platform`, `RHI` (Metal + DX11), `Reflection`, `Serialization`, `Assets`, `Geometry`, `Level`, `Rendering`, `Physics` (Jolt), `UI`, `Framework`, `Runtime`. Each module is its own CMake object library |
| `Engine/Content` | Runtime content shipped with games: HLSL shaders, fonts. `Shaders/ShaderBindings.h` holds every shader slot number for both HLSL and C++. `Shaders/ThirdParty` is fetched at configure time and gitignored |
| `Editor/Source` | Editor. Built as the static lib `BrightEditorCore` plus the `BrightEditor` exe. `Panels/` holds dockable windows, `Services/` the editor services owned by `EditorContext`, `ImGui/` the backends |
| `Editor/Content` | Editor-only content. `Fonts/ThirdParty/Lucide` (icon font) is fetched at configure time and gitignored |
| `Player/Source` | `BrightPlayer`, the standalone runtime used by packaged games |
| `Tools/BrightHeaderTool` | Reflection code generator. It writes a `.gen.cpp` per reflected header |
| `Tests/<Area>` | GoogleTest suite `BrightTests`. `System/` builds and runs a full packaged game |
| `Samples/ArenaDemo` | Sample game project, used by the system tests |
| `cmake/` | `Dependencies.cmake` (FetchContent), `BrightReflection.cmake`, `BrightGame.cmake` (game module builds), `ModuleDependencies.cmake` (allowed module includes) and `CheckModuleDependencies.cmake` (the build-time check) |

## Tech stack

- **Build:** CMake 3.24+, FetchContent only, nothing vendored by hand. Outputs go to `build/bin`.
- **Libraries:** GLFW 3.4, RapidJSON, stb, ufbx, Dear ImGui (docking) + ImGuizmo, pugixml, Yoga, FreeType + msdf-atlas-gen, Jolt Physics, SMAA, Lucide icons, glslang + SPIRV-Cross (macOS only), Ninja (builds game modules), GoogleTest.
- **Rendering:**
  - Shaders are written once in HLSL. FXC compiles them on Windows; glslang → SPIRV-Cross translates them to Metal on macOS.
  - Math is row-vector with row-major matrices, left-handed, Y up.
- **Platforms:** macOS/Metal is built and tested. Windows/DX11 code exists but has never been compiled.

## Build, run, test

```bash
cmake -S . -B build
cmake --build build -j
./build/bin/BrightTests
./build/bin/BrightEditor Samples/ArenaDemo
```

To regenerate the sample's authored assets (Level, Player Variant) after changing the system test: `BRIGHT_EXPORT_SAMPLE=1 ./build/bin/BrightTests --gtest_filter='SystemTests.*'`.

Editor automation flags, useful for visual checks: `--select <name>`, `--play`, `--build`, `--frames N`, `--screenshot <png>`, `--play-screenshot <png>`, `--create-project`. To check UI work, take a screenshot and look at it:

```bash
./build/bin/BrightEditor Samples/ArenaDemo --select Crate --frames 60 --screenshot /tmp/shot.png
```

## Architecture

- **Object model:** `Reflection::EngineObject` is the root of every reflected class and declares the lifecycle callbacks once. `Level::LevelObject` lives in the world and owns `Level::LevelObjectComponent`s. `Framework::PlayableLevelObject` is possessed by a `Framework::PlayerController`. `GameModeBase`, `GameStateBase`, `PlayerController`, `Level::LevelBehavior` and level subsystems are Level-owned EngineObjects, not world objects.
- **Gameplay settings** live per Level (`gameMode` and `levelBehavior` in the Level's settings), read through `Framework::LevelGameplaySettings`.
- **Assets:** `LevelObjectTemplate` (`.template`) can't be placed; `LevelObjectVariant` (`.variant`) is placed or spawned (`Level::Spawn`) and may be based on a Template.
- **Hot reload:** engine-owned state lives in `EngineObjectState`, `LevelObjectState` and `ComponentState`, moved whole by `ObjectReplacer`. Raw pointers between objects survive a reload only through `CarryReferences`.
- **Module boundaries:** `cmake/ModuleDependencies.cmake` is the single source of truth; the build fails on a forbidden `#include`. Add a module there before using it.
- **Renamed classes:** add a `Reflection::TypeRedirectRegistration` so saved files with the old name still load.

## Code conventions

- **No comments in code**, anywhere.
- **Naming:**
  - PascalCase for types, functions, enum values and constants.
  - camelCase for locals and parameters.
  - `m_` prefix for members.
  - Allman braces, 4-space indent.
- **Namespaces:** `BrightBox3D::<Module>` (for example `BrightBox3D::Rendering`); editor code goes in `BrightBox3D::Editor`.
- **Includes:**
  - In a `.cpp`, the file's own header comes first.
  - Then project headers, alphabetical, as `"Module/File.h"`.
  - Then third-party headers (`<imgui.h>`), then std headers.
- **Singletons** use `static T& Get()`, backed by `static T* x = new T();`.
- **Reflection:**
  - `BrightClass(...)`, `BrightBody()`, `BrightProperty(EditAnywhere|..., Category = "...")`, `BrightUIProperty(Read|Write)`, `BrightEnum()`.
  - CMake generates `.gen.cpp` files; never edit them.
- **Editor colors:** never hard-code a color. Add an entry to `EditorColor` / `ColorInfos` in `Editor/Source/EditorPreferences.*` and read it with `EditorColorVec4` / `EditorColorU32`. Theme-wide ImGui colors are derived in `Editor/Source/Theme.cpp`.
- **Editor icons:** use the Lucide constants in `Editor/Source/Icons.h`. To add one, look up its codepoint in `lucide-static`'s `font/info.json`.
- **Selection highlight:** wrap list and tree panels in `PushSelectionColors()` / `PopSelectionColors()`.

## Workflow rules

- Follow the owner's global rules. Ask instead of assuming when a design decision isn't specified.
- Present changes for review, and commit only after explicit approval.
- Commits are small and logically scoped, with short messages and no AI attribution.
- Run `BrightTests` before presenting work. For editor UI changes, also check a screenshot.
