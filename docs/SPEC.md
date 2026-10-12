# BrightBox3D — Specification (v1.0 draft for review)

Status: **all known questions resolved; awaiting owner review** before implementation starts.

---

## 1. Vision

BrightBox3D is a 3D-only game engine written entirely in C++: engine, editor and gameplay code. It is opinionated like Unreal Engine — the engine provides a gameplay framework (GameMode, PlayerController, ...) that shapes how games are structured — while the editor UI and workflow are modelled on Unity. Iteration speed is a first-class goal: gameplay C++ is hot-reloaded reliably while the editor runs.

PrismEngine (`../PrismEngine`) was a prototype (Unity clone, C# runtime, OpenGL 4.1, macOS only). BrightBox3D is a new engine with stricter requirements; Prism code may be referenced but is not assumed to be reused.

## 2. Requirements

| # | Requirement |
|---|---|
| R1 | Engine, editor and gameplay code are C++20. No managed or scripting runtime. |
| R2 | 3D games only. |
| R3 | Rendering through an engine RHI with **DirectX 11** (Windows) and **Metal** (macOS) backends. |
| R4 | Modern rendering: deferred PBR, post-processing, CSM shadows, FXAA/SMAA/TAA. |
| R5 | Unreal-style opinionated gameplay framework. |
| R6 | Reliable hot reload of gameplay C++ while the editor runs. |
| R7 | Editor UI/UX modelled on Unity. |
| R8 | Engine macros/attributes use the `Bright` prefix. Engine classes are unprefixed, inside `BrightBox3D::<Module>` namespaces. |
| R9 | Unreal-style reflection attributes on C++ members (`BrightProperty(...)`), processed by a custom header tool. |
| R10 | Screen-space UI is a widget system (UI Toolkit-like). World-space UI is built from LevelObjects. |
| R11 | UI reactivity via `BrightUIProperty` with Read/Write permissions. |
| R12 | Source assets are human- and AI-readable text, diffable in git; runtime loads fast cooked binaries. |

## 3. Technology

| Concern | Choice |
|---|---|
| Language | C++20 |
| Build system | CMake |
| Platform layer | GLFW |
| Engine linkage | Engine core is a shared library (`BrightBox3D.dll` / `libBrightBox3D.dylib`) used by editor, player and gameplay module |
| Shaders | HLSL (SM 5.0). DX11: FXC (`D3DCompile`). Metal: glslang (HLSL → SPIR-V) → SPIRV-Cross (→ MSL) → runtime MSL compile |
| Graphics | Engine RHI; DX11 backend (Windows), Metal backend (macOS) |
| Editor UI | Dear ImGui (docking branch) + ImGuizmo |
| Reflection | Custom header tool (`BrightHeaderTool`) |
| Hot reload | Gameplay shared-library swap |
| Physics | Jolt Physics |
| Model import | FBX via ufbx |
| UI layout | Yoga (Flexbox) |
| Math | Own `BrightBox3D::Math` |
| Source asset format | JSON + GUIDs |
| JSON | RapidJSON |
| XML (UI markup) | pugixml |
| Images | stb_image |
| File watching | Own (FSEvents on macOS, ReadDirectoryChangesW on Windows) |
| Fonts | msdf-atlas-gen + FreeType |
| Tests | GoogleTest |
| UI text | MSDF font atlases |

Development environment: macOS. Both backends are written together; only Metal is built and verified here. The DX11 backend is built and tested by the owner on Windows.

## 4. Conventions

### 4.1 Naming

- Types, methods, enum values, namespaces: **PascalCase**.
- Locals and parameters: **camelCase**.
- Private/protected members: **`m_` + camelCase**.
- Engine macros: **`Bright` + PascalCase** (`BrightClass`, `BrightBody`, `BrightProperty`, `BrightUIProperty`).
- Namespaces: `BrightBox3D::<Module>` — e.g. `BrightBox3D::Core`, `BrightBox3D::Math`, `BrightBox3D::RHI`, `BrightBox3D::Level`, `BrightBox3D::UI`, `BrightBox3D::Editor`.
- No comments in engine code.

```cpp
namespace BrightBox3D::Level {
class LevelObjectInstance {
public:
    virtual void OnGameStart();
    Transform& GetTransform();
private:
    Transform m_transform;
    bool m_isActive = true;
};
}
```

### 4.2 Coordinate system

DirectXMath native: **left-handed, +Y up, +Z forward, +X right**. Same on both backends.

## 5. Terminology

Binding for code, UI and documentation.

| Unity term | BrightBox3D term | Notes |
|---|---|---|
| Editor | Editor | |
| Package Manager | Package Manager | Name reserved; not in v1. |
| Prefab Variant | **LevelObjectTemplate** | Authoring-only. Used to create LevelObjects. Cannot be spawned. |
| Prefab | **LevelObject** | Spawnable asset. |
| GameObject | **LevelObjectInstance** | An object in a loaded Level. |
| Scene | **LevelCollection** | Async loadable/unloadable group of objects. |
| *(new)* | **Level** | Top-level "scene"; contains LevelCollections. |
| Play Mode | PlayMode | |
| Console | **Terminal** | |
| Scene view | **Level view** | |
| Game view | *(none)* | PlayMode runs in a separate window. |
| Hierarchy | **Level Contents** | |
| Project window | **Assets** | |
| Inspector, Transform, Component, Gizmo, Toolbar | same | |

## 6. Levels and assets

### 6.1 Level structure

```
Level                      only one open in the editor at a time
 ├── LevelCollection A     async load / unload
 │    ├── LevelObjectInstance
 │    └── LevelObjectInstance
 └── LevelCollection B
      └── ...
```

- One Level open at a time; no multi-Level editing.
- LevelObjectInstances can be **parented** to each other (Unity-style); child Transforms are relative to the parent. LevelObjects can contain child objects.
- LevelCollections exist for streaming large maps. v1 loads/unloads them via **gameplay API** (`LoadCollectionAsync` / `UnloadCollection` with completion callback).

### 6.2 Template → LevelObject → Instance

```
LevelObjectTemplate --create from--> LevelObject --place/spawn--> LevelObjectInstance
   (not spawnable)                     (spawnable)                  (lives in a Level)
```

| Relationship | Behaviour |
|---|---|
| Template → LevelObject | **One-time copy.** The LevelObject is independent afterwards. |
| LevelObject → Instance | **Linked with overrides.** LevelObject edits propagate except overridden values. Inspector shows overrides in bold with **Apply** and **Revert**. |

### 6.3 Storage

| Tier | Format | In git | Purpose |
|---|---|---|---|
| Source | JSON; each asset has a GUID in a `.meta` sidecar | Yes | Source of truth. Editable by editor, hand or AI. |
| Cooked | Engine binary in `Cache/` | No | Fast loading in editor and builds. |

- Cooked entries store their source hash; only stale entries are re-cooked, on asset change and on project open.
- Packaged builds contain cooked binaries only.

## 7. Gameplay framework

### 7.1 Object model

- `LevelObjectInstance` is the root gameplay class (like `AActor`). Every object in a Level is one, or a subclass.
- Each object has one **brain class** describing what it is; inheritance chains allowed (`Player : PlayableCharacter : Character : LevelObjectInstance`).
- Each object also holds **Components**, like a Unity GameObject.
- v1 ships `LevelObjectInstance` as the base; `Character`/`PlayableCharacter` come later.

```
Player  (brain)  : PlayableCharacter : Character : LevelObjectInstance
 ├── Transform
 ├── PlayerController   component
 ├── MeshRenderer       component
 └── ...
```

### 7.2 Lifecycle

Same names on `LevelObjectInstance` and Components. `OnGameUpdateFixed` and physics run at a fixed **60 Hz** by default (project setting).

| BrightBox3D | Unity | Unreal |
|---|---|---|
| `OnInitialize` | — | `OnConstruction` |
| `OnGameLoad` | `Awake` | — |
| `OnGameStart` | `Start` | `BeginPlay` |
| `OnGameUpdate` | `Update` | `Tick` |
| `OnGameUpdateFixed` | `FixedUpdate` | — |
| `OnGameUpdateLate` | `LateUpdate` | — |
| `OnEnable` | `OnEnable` | — |
| `OnDisable` | `OnDisable` | — |
| `OnGameEnd` | — | `EndPlay` |
| `OnDestroy` | `OnDestroy` | `Destroyed` |

### 7.3 Framework classes

| Class | Kind | Role |
|---|---|---|
| `GameMode` | Engine class, subclassable | Rules and basic game logic. Exposes `DefaultPlayer` (LevelObject ref). Spawns players on PlayMode start. |
| `PlayerController` | Component | Makes its object player-controlled. |
| `Camera` | `LevelObjectInstance` subclass | Spawned and **owned** by the player object. Has an **Active UI Screen**. |
| `PlayerStart` | `LevelObjectInstance` subclass | Spawn point. |

GameMode selection: default in project settings, **overridable per Level**. On PlayMode start the GameMode spawns `DefaultPlayer` at a `PlayerStart`.

### 7.4 Input

v1: raw keyboard and mouse polling (key down/pressed/released, mouse position/delta/buttons).

## 8. Reflection

### 8.1 Syntax

```cpp
BrightClass()
class Player : public PlayableCharacter {
    BrightBody()
public:
    void OnGameStart() override;
private:
    BrightProperty(EditDefaultsOnly, Category = "Movement")
    float m_moveSpeed = 600.0f;

    BrightUIProperty(Read)
    int m_health = 100;
};
```

- `BrightClass()` marks a reflected class (brain classes and components). `BrightBody()` injects generated code.
- `BrightProperty`/`BrightUIProperty` expand to nothing for the compiler; `BrightHeaderTool` parses headers before compilation and generates reflection code.
- Reflected names are the **raw C++ member name** (`m_moveSpeed`) in JSON and UI binding paths. Only the Inspector label is prettified ("Move Speed").

### 8.2 `BrightProperty` specifiers (v1)

| Specifier | Meaning |
|---|---|
| `EditAnywhere` | Editable on LevelObjects and instances. |
| `EditDefaultsOnly` | Editable only on the LevelObject asset. |
| `EditInstanceOnly` | Editable only on placed instances. |
| `VisibleAnywhere` | Shown read-only. |
| `Hidden` | Reflected and serialized, not shown. |
| `Category = "..."` | Inspector grouping. |

## 9. UI system

### 9.1 Concepts

| Concept | Description |
|---|---|
| Widget | Reusable UI component: button, field, health bar, "Create Room", "Leave Game", ... |
| Screen | Container of widgets. Each Camera has one Active UI Screen. |
| World-space UI | LevelObjects, not widgets. |

### 9.2 Authoring

Each Widget/Screen = **XML markup** + **CSS-subset stylesheet** + optional **C++ class**. Layout is **Flexbox (Yoga)**. Text uses **MSDF font atlases** (msdf-atlas-gen).

### 9.3 `BrightUIProperty` bindings

| Permission | Widget may |
|---|---|
| `Read` | Observe; UI updates on change. |
| `Write` | Observe and change. |

- Bindings are **paths in markup**, resolved via reflection and validated against the permission (Write binding to a Read property is an error).
- Bindable sources (v1): the **owning player** (object owning the Camera showing the Screen) and its components; the active **GameMode**.

## 10. Rendering

- Engine RHI with DX11 and Metal backends.
- **Deferred** opaque pipeline, **PBR** metallic/roughness.
- **Transparent** objects: forward pass after deferred lighting, sorted back-to-front, same lights.
- **Cascaded shadow maps** for directional lights.
- Lights: **directional** (CSM shadows), **point**, **spot** (no shadows in v1).
- **Post-processing**: HDR, tonemapping, exposure (manual + auto), bloom, color grading, vignette, SSAO.
- **Materials**: property-based. A material asset (JSON) = shader + parameter values (albedo, metallic, roughness, normal, emissive, ...), edited in the Inspector.
- Anti-aliasing: **FXAA**, **SMAA**, **TAA**, selectable.

## 11. Physics

Jolt Physics.

### 11.1 Built-in components (v1)

| Component | Purpose |
|---|---|
| `Transform` | Position/rotation/scale, parenting |
| `MeshRenderer` | Draws a mesh with materials |
| `Light` | Directional / point / spot |
| `Rigidbody` | Jolt body: dynamic / kinematic / static |
| `BoxCollider`, `SphereCollider`, `CapsuleCollider`, `MeshCollider` | Shapes; trigger flag; mesh = convex or static triangle mesh |
| `PlayerController` | Player control |

## 12. Hot reload

- Gameplay code builds into a separate **shared library**.
- Reload: serialize reflected state of gameplay objects → destroy them → unload old library → load new → recreate → restore state via reflection. No binary patching.
- Trigger: **automatic on source save** (can be disabled) + **manual Toolbar button/shortcut**.
- Compile failure: errors go to the Terminal; running PlayMode is **stopped**; the previously loaded library stays loaded.

## 13. Editor

### 13.1 Layout

Unity-like docked layout: Toolbar, Level view, Level Contents, Inspector, Assets, Terminal.

### 13.2 Features (v1)

- Project launcher on startup (create / open project).
- Undo/Redo for all edits.
- Gizmos: move / rotate / scale, local/world, snapping.
- Assets panel with thumbnails; drag-drop LevelObjects into Level view / Level Contents.
- Reflection-driven Inspector with override display, Apply/Revert.

### 13.3 PlayMode

- Runs in a **separate window inside the editor process**; editor keeps the Level view. The Inspector shows live PlayMode objects, and hot reload works during play.
- Runs on a copy of the Level; changes **revert on stop**, except values marked **Keep changes**.

## 14. Projects and packaging

- A game project has `Assets/` (source assets) and `Source/` (gameplay C++). The editor **generates a CMake project** for `Source/`; the gameplay module builds as a shared library against the engine SDK. Works in Visual Studio, CLion, Xcode.
- **Build** produces an output folder: runtime player executable, gameplay module, cooked assets.

## 15. Out of scope for v1

Ray tracing (needs DX12) · skeletal animation · audio · networking · model formats other than FBX · input action mapping and gamepad · image-based lighting · point/spot light shadows · Package Manager · LevelCollection streaming volumes · multi-Level editing · node-graph materials · area lights · character controller component · DoF and motion blur.

## 16. Milestones (proposed)

"Usable" = all milestones complete.

| # | Milestone | Contents | Verified by |
|---|---|---|---|
| M0 | Foundation | Repo, CMake, platform layer (window, timing, file system, input), logging, `BrightBox3D::Math` | Unit tests, window opens |
| M1 | Reflection | Reflection runtime, `BrightHeaderTool`, JSON serialization, GUIDs | Round-trip tests on reflected types |
| M2 | Object model | `LevelObjectInstance`, Component, Transform hierarchy, lifecycle, Level, LevelCollection, LevelObject/Template assets, overrides | Headless tests |
| M3 | RHI + assets | RHI, Metal + DX11 backends, shader pipeline, asset database, cooking, FBX import, basic mesh draw | Mesh renders on Metal |
| M4 | Editor shell | ImGui docking, launcher, Level view, Level Contents, Inspector, Assets, Terminal, Toolbar, undo/redo, gizmos | Create/edit/save a Level |
| M5 | Hot reload | Project generation, gameplay module build, file watcher, DLL swap with state restore | Edit code → change visible without restart |
| M6 | Gameplay + PlayMode | GameMode, PlayerController, Camera, PlayerStart, input, PlayMode window, revert/keep changes | Player spawns and moves |
| M7 | Renderer | Deferred PBR, lights, forward transparency, CSM, post-processing, FXAA/SMAA/TAA | Visual test scenes |
| M8 | Physics | Jolt integration, physics components, collision callbacks, raycasts | Physics test Level |
| M9 | UI | XML/CSS parsing, Yoga layout, widget rendering, text, Screens, `BrightUIProperty` bindings | HUD bound to player health |
| M10 | Streaming + packaging | Async LevelCollection load/unload, Build command, runtime player | Packaged game runs |
