# BrightBox3D — Specification (v1.0 draft for review)

Status: **implemented (milestones M0-M10)**; see §17 for implementation decisions made during development.

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
class LevelObject {
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
| Prefab (base) | **LevelObjectTemplate** | Base asset. Cannot be placed or spawned. |
| Prefab / Prefab Variant | **LevelObjectVariant** | Placeable and spawnable asset, optionally based on a Template. |
| GameObject | **LevelObject** | An object in a loaded Level. |
| Component | **LevelObjectComponent** | Behavior attached to a LevelObject. |
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
 │    ├── LevelObject
 │    └── LevelObject
 └── LevelCollection B
      └── ...
```

- One Level open at a time; no multi-Level editing.
- LevelObjects can be **parented** to each other (Unity-style); child Transforms are relative to the parent. LevelObjects can contain child objects.
- LevelCollections exist for streaming large maps. v1 loads/unloads them via **gameplay API** (`LoadCollectionAsync` / `UnloadCollection` with completion callback).

### 6.2 Template → Variant → placed LevelObject

```
LevelObjectTemplate --base of--> LevelObjectVariant --place/spawn--> LevelObject
   (not placeable)                  (placeable)                     (lives in a Level)
```

| Relationship | Behaviour |
|---|---|
| Template → Variant | **Linked with overrides.** A Variant may have no Template; one level only, no Variant of a Variant. |
| Variant → placed LevelObject | **Linked with overrides.** Edits propagate except overridden values. The Inspector shows overrides with **Apply** and **Revert**. |

### 6.3 Storage

| Tier | Format | In git | Purpose |
|---|---|---|---|
| Source | JSON; each asset has a GUID in a `.meta` sidecar | Yes | Source of truth. Editable by editor, hand or AI. |
| Cooked | Engine binary in `Cache/` | No | Fast loading in editor and builds. |

- Cooked entries store their source hash; only stale entries are re-cooked, on asset change and on project open.
- Packaged builds contain cooked binaries only.

## 7. Gameplay framework

### 7.1 Object model

- `EngineObject` is the root of every reflected class and declares the lifecycle callbacks.
- `LevelObject` is the in-world gameplay class (like `AActor`). Each object has one **brain class** describing what it is, and holds **LevelObjectComponents**, like a Unity GameObject.
- `PlayableLevelObject` is a LevelObject a `PlayerController` can possess.

```
EngineObject
├── LevelObject
│   ├── PlayableLevelObject (possessed by a PlayerController)
│   ├── Camera, PlayerStart
├── LevelObjectComponent (Transform, MeshRenderer, Light, colliders, Rigidbody)
├── LevelBehavior, LevelSubsystem
├── GameModeBase, GameStateBase, PlayerController
└── asset classes
```

### 7.2 Lifecycle

Declared once on `EngineObject`; delivered to LevelObjects, components and Level-owned EngineObjects. `OnGameUpdateFixed` and physics run at a fixed **60 Hz** by default (project setting).

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
| `GameModeBase` | Level-owned EngineObject, subclassable | Rules. Creates the GameState and PlayerControllers, spawns `DefaultPlayable` at a `PlayerStart` and possesses it, respawns on `OnPlayableDestroyed`. |
| `GameStateBase` | Level-owned EngineObject, subclassable | Game data. |
| `PlayerController` | Level-owned EngineObject | Possesses one `PlayableLevelObject`; input, look and view target. |
| `LevelBehavior` | Level-owned EngineObject, optional | Per-Level scripting. |
| `Camera` | `LevelObject` subclass | Usually a child of the Playable. Has an **Active UI Screen**. |
| `PlayerStart` | `LevelObject` subclass | Spawn point. |

Each Level chooses its GameMode class and settings and optionally a LevelBehavior; there is no project-wide GameMode. Start order: LevelBehavior, GameMode, GameState, PlayerControllers, spawned players; load and start callbacks follow that order and end runs in reverse.

### 7.4 Input

v1: raw keyboard and mouse polling (key down/pressed/released, mouse position/delta/buttons).

## 8. Reflection

### 8.1 Syntax

```cpp
inline constexpr BrightBox3D::Reflection::Category Movement{ "Movement" };

BrightClass(.category = Categories::Gameplay, .icon = Icon::User)
class Player : public BrightBox3D::Framework::PlayableLevelObject {
    BrightBody()
public:
    void OnGameStart() override;
private:
    BrightProperty(.edit = Edit::DefaultsOnly, .category = Movement)
    float m_moveSpeed = 6.0f;

    BrightUIProperty(.access = UIAccess::Read)
    int m_health = 100;
};

BrightEnum()
enum class LightType { Directional, Point, Spot };
```

- Macro arguments are C++20 designated initializers of typed metadata structs. The compiler checks them at the user's line and the IDE autocompletes fields (`.edit`, `.category`) and values (`Edit::`, `Icon::`, `Categories::`). A misspelled field, enum value or category is a compile error.
- Fields must be written in declaration order:

| Macro | Struct | Fields, in order |
|---|---|---|
| `BrightClass` | `ClassMetadata` | `category`, `icon`, `abstract` |
| `BrightProperty` | `PropertyMetadata` | `edit`, `category` |
| `BrightUIProperty` | `UIPropertyMetadata` | `access` |
| `BrightEnum` | `EnumMetadata` | none yet |

- `Icon`, `Edit`, `UIAccess` and `Categories` are in scope inside the macros, so they are written unqualified. Game names (like `Movement` above) resolve from the surrounding namespace.
- `BrightBody()` injects generated declarations. `BrightHeaderTool` copies the macro arguments verbatim into the generated `.gen.cpp`, where the compiler evaluates them; it only reports structural errors and the old token syntax.
- Reflected names are the **raw C++ member name** (`m_moveSpeed`) in JSON and UI binding paths. Only the Inspector label is prettified ("Move Speed").

### 8.2 Edit modes and categories

| `Edit::` | Meaning |
|---|---|
| `None` (default) | Reflected and saved, not shown. |
| `Anywhere` | Editable on Templates, Variants and placed instances. |
| `DefaultsOnly` | Editable only on the Template or Variant asset. |
| `InstanceOnly` | Editable only on placed instances. |
| `Visible` | Shown read-only. |
| `Hidden` | Reflected and saved, never shown. |

- A category is a `constexpr Reflection::Category` value declared once, in one line. The engine's live in each module's `Categories.h` (`Categories::Gameplay`, `Categories::Physics`, …); games declare their own the same way.
- `.icon` takes any Lucide icon. The editor shows it in Level Contents, the Add Component menu and Inspector headers. An object's icon is its class icon (inherited), else its first component with an icon, else a default.

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
- Bindable sources: `Player` (the viewing controller's Playable) and its components, `GameMode`, `GameState`, `Controller`, `LevelBehavior`.

## 10. Rendering

- Engine RHI with DX11 and Metal backends.
- **Deferred** opaque pipeline, **PBR** metallic/roughness.
- **Transparent** objects: forward pass after deferred lighting, sorted back-to-front, same lights.
- **Cascaded shadow maps** for directional lights.
- Lights: **directional** (CSM shadows), **point** and **spot** (shadows from a shared depth atlas).
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
| `Collider` | One component; `ColliderShape` Box (Size), Sphere (Diameter) or Mesh (the object's `MeshSource` mesh unless a model is set, scaled by Size; convex or static triangle mesh); trigger flag; friction and restitution |
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

Ray tracing (needs DX12) · skeletal animation · audio · networking · model formats other than FBX · input action mapping and gamepad · image-based lighting · Package Manager · LevelCollection streaming volumes · multi-Level editing · node-graph materials · area lights · character controller component · DoF and motion blur.

## 16. Milestones (proposed)

"Usable" = all milestones complete.

| # | Milestone | Contents | Verified by |
|---|---|---|---|
| M0 | Foundation | Repo, CMake, platform layer (window, timing, file system, input), logging, `BrightBox3D::Math` | Unit tests, window opens |
| M1 | Reflection | Reflection runtime, `BrightHeaderTool`, JSON serialization, GUIDs | Round-trip tests on reflected types |
| M2 | Object model | `LevelObject`, Component, Transform hierarchy, lifecycle, Level, LevelCollection, LevelObject/Template assets, overrides | Headless tests |
| M3 | RHI + assets | RHI, Metal + DX11 backends, shader pipeline, asset database, cooking, FBX import, basic mesh draw | Mesh renders on Metal |
| M4 | Editor shell | ImGui docking, launcher, Level view, Level Contents, Inspector, Assets, Terminal, Toolbar, undo/redo, gizmos | Create/edit/save a Level |
| M5 | Hot reload | Project generation, gameplay module build, file watcher, DLL swap with state restore | Edit code → change visible without restart |
| M6 | Gameplay + PlayMode | GameMode, PlayerController, Camera, PlayerStart, input, PlayMode window, revert/keep changes | Player spawns and moves |
| M7 | Renderer | Deferred PBR, lights, forward transparency, CSM, post-processing, FXAA/SMAA/TAA | Visual test scenes |
| M8 | Physics | Jolt integration, physics components, collision callbacks, raycasts | Physics test Level |
| M9 | UI | XML/CSS parsing, Yoga layout, widget rendering, text, Screens, `BrightUIProperty` bindings | HUD bound to player health |
| M10 | Streaming + packaging | Async LevelCollection load/unload, Build command, runtime player | Packaged game runs |

## 17. Implementation notes

Decisions made during implementation that the earlier sections did not pin down.

### 17.1 Reflection

- `BrightClass(.abstract = true)` marks classes that cannot be instantiated; `.category` groups types in the editor's Add Component and create menus.
- `BrightEnum()` reflects an `enum` / `enum class` so it can be used as a property (shown as a dropdown, saved by name).
- A member with only `BrightUIProperty(...)` is reflected and saved but not shown in the Inspector.
- Supported property types: `bool`, `int32_t`, `uint32_t`, `int64_t`, `float`, `double`, `std::string`, `Vector2/3/4`, `Quaternion`, `Color`, `Guid`, reflected enums, `AssetReference<T>`, `ObjectReference<T>`, `ClassReference<T>` and `std::vector` of these.
- The `Icon` enum is generated at configure time from Lucide's `info.json` into `build/Generated/Engine/Core/Icon.h`; the editor's glyph table goes to `build/Generated/Editor/IconGlyphs.h`.
- Engine and game targets build with `-Werror=reorder-init-list`, so out-of-order metadata fields are errors on AppleClang too. `BrightTests` compiles small snippets to prove each kind of mistake fails.

### 17.2 Objects and levels

- Every `LevelObject` owns a `Transform` component that cannot be removed.
- Physics callbacks were added to the lifecycle: `OnCollisionEnter`, `OnCollisionExit`, `OnTriggerEnter`, `OnTriggerExit` (on both instances and components).
- All LevelCollections listed in a Level load when the Level opens. Streaming at runtime: `Level::LevelStreaming::LoadAsync(level, collection, callback)` / `Unload(level, collection)`.
- Linked LevelObject instances accept property overrides only. Adding or removing components or moving objects inside an instance requires **Unlink** or editing the LevelObject itself (**Open**). Creating a LevelObject from objects that contain other linked instances copies them (no nested LevelObjects in v1).
- Game classes and components that disappear after a code change are kept as *missing types* with their data preserved and restored when the class returns.
- Hot reload preserves reflected state, ids, hierarchy, components and lifecycle state; non-reflected members are reset to their defaults.

### 17.3 Rendering

- HLSL is compiled with FXC on Windows and translated with glslang → SPIRV-Cross on macOS (DXC is unavailable on macOS and cannot target DX11).
- Ambient light comes from a sky gradient configured on the Camera (no image-based lighting in v1).
- SMAA uses the reference implementation (iryoku/smaa, MIT), fetched at configure time.
- The editor Level view uses FXAA; Cameras default to TAA.

### 17.4 UI

- Binding paths: `{Player.member}`, `{Player.ComponentType.member}`, `{GameMode.member}`, `{GameState.member}`, `{Controller.member}`, `{LevelBehavior.member}`; `{path:N}` formats with N decimals.
- `Slider value`, `Toggle checked` and `TextField text` bindings are two-way and require `BrightUIProperty(Write)`. `Button onClick="{path}"` sets a bool to true or increments a number (Write required).
- Elements: `Panel`, `Label`, `Button`, `Image`, `TextField`, `ProgressBar`, `Slider`, `Toggle`, plus `<Use widget="path"/>` and `<Style src="path"/>`. A root `controller="ClassName"` attribute attaches a C++ `UI::WidgetController`.
- The default UI font is Roboto Medium (Apache 2.0) rendered as an MSDF atlas.

### 17.5 Editor and packaging

- The project launcher uses a typed path instead of native file dialogs.
- During PlayMode the Level view shows the live play world; the game itself renders in the separate PlayMode window.
- **Build Game** writes `<Project>/Build/<Platform>/` containing the player executable (named after the game module), the engine library, the game module, `EngineContent/`, cooked assets and `Game.json`.
- Editor automation flags (used by tests): `--create-project`, `--select`, `--play`, `--build`, `--frames`, `--screenshot`, `--play-screenshot`, `--add-panel`.
- All editor UI text comes from the `EditorText` table (`Editor/Source/EditorText.h/.cpp`), formatted with `std::format` and ready for translation through `EditorTextTable::LoadOverrides`. Window titles carry fixed `###Id`s (`EditorWindow`), so the saved dock layout survives title changes.
- Toolbar buttons, menu items and shortcuts are commands (`CommandRegistry`, defined in `EditorCommands.cpp`). Shortcuts have a scope (Global or a panel); a panel's shortcut wins while it is hovered or focused. They are rebindable on the Preferences Shortcuts page and saved under `"shortcuts"` in `Preferences.json`.
- Spec 0001: the Assets panel has One Column and Two Column layouts (grid, or list at the slider's leftmost stop), breadcrumbs, a bottom bar, type icons, and one right-click menu (`AssetsContextMenu::Draw`) backed by `CommandScope::Assets` commands. Layout and icon size are saved under `"assets"` in `Preferences.json`.
- Logs carry a `LogSource` (Engine or Game). Game code uses `BrightBox3D::Debug`. The Terminal shows game messages under Info/Warnings/Errors and engine messages under an Engine toggle (colored by its worst severity); the toggles are saved under `"terminal"`.
- Dockable panels derive from `EditorPanel` and can be opened more than once (tab `+`, Window menu). Each Level View has its own camera and renderer view id, each Inspector can be locked, each Assets panel has its own folder. Open panels and their state are saved to `~/.brightbox3d/EditorPanels.json`; first instances keep the old window names so existing `EditorLayout.ini` files still dock.
- Asset moves keep the GUID (a meta whose previous file is gone is a move, not a copy). `AssetDatabase::DuplicateAsset` copies files and folders with new GUIDs.

### 17.6 Verification status

- Everything is built and tested on macOS/Metal (unit, rendering, editor, hot reload and end-to-end packaging tests).
- The DirectX 11 backend and Windows-specific code (file watcher, process launching, DLL loading) are written but have not been compiled or run.

### 17.7 Architecture (spec 0002)

- Module includes are restricted by `cmake/ModuleDependencies.cmake` and checked at build time. Modules: Core, Math, Platform, Reflection, Serialization, Assets, Geometry (meshes, models, the FBX importer), Level, RHI, Rendering, Physics, UI, Framework, Runtime.
- `TypeInfo` carries its module name from `BRIGHT_MODULE_NAME`, set per module target.
- Hot reload moves `EngineObjectState`, `LevelObjectState` and `ComponentState` whole and replaces Level-owned EngineObjects too; object pointers survive through `CarryReferences`.
- Old class and asset names are migrated: `TypeRedirects` (e.g. `LevelObjectInstance`, `GameMode`), `.levelobject` → `.variant` and the project GameMode → per-Level settings on project open.
- Shader slots are named in `Engine/Content/Shaders/ShaderBindings.h`, shared by HLSL and C++. `Renderer::Render` runs a list of `RenderPass` classes sharing a `FrameContext` (spec 0004).
- The editor's `EditorContext` owns services: Project, Level, Selection, Undo, LevelObjectEdit, AssetCreation and ObjectEdit.
- Game code compiles with Ninja (built with the editor) so saves right after a build are always picked up. A compile can be cancelled; the module is built into `Binaries/Staging` and copied into place only after success.

### 17.8 Code quality (spec 0004)

- Large files are split by responsibility: editor application, Inspector, Level Contents, Assets and Level View panels, `PropertyDrawer` (`ValueDrawers`), `Renderer` (`RenderPass` classes), `UI::Document`, `Level`, `PhysicsWorld`, `LevelSerializer`, `AssetDatabase`, the rendering importers, the Metal and DX11 devices, and the Player (`PlayerApplication`).
- Shared helpers replace duplicates: `DragDrop`, `InlineRename`, `MipGenerator`, `ColorSpace`, `AssetDatabase::IsIgnoredPath`, `Core::Widen` and `Tests/Common/TestUtilities`.
- Import settings are reflected structs (`TextureImportSettings`, `ModelImportSettings`) passed to `Cook` and drawn by the Inspector.
- The create menu is data driven (`CreateMenuRegistry`); reflected placeable classes appear by category. The default dock layout is a table, and editor sizes are `EditorMetric` entries.
- Level pseudo-properties (name, active, enabled) use one `PseudoProperty` descriptor table.
- The DX11 split was checked only by comparing source lines; it compiles only through the Windows workflow.

### 17.9 String cleanup (spec 0005)

- Editor text naming engine types formats the type name in (`TypeName<T>()`, `AssetTypeName`). Bare type labels use the name directly.
- `Reflection::DisplayName` metadata (`.displayName`) overrides a property's prettified name; Transform shows Position/Rotation/Scale through it. `RenderDebugMode` is reflected and the Level View lists its entries.
- Paths: `Core::EngineLayout`, `Editor::EditorLayout`, `Runtime::ProjectLayout::LevelsFolder`/`LevelAssetPath`, `BuildLayout::PlatformFolderName`. `FileSystem::FindContentDirectory` serves both the engine and editor content search.
- `AssetType` table holds default new-asset names (EngineText) and creatability; `MatchedExtension` drives rename. Built-ins are `Builtin/<Name>` from the `BuiltinPrimitive` table.
- Default names are text (`EngineText`/`EditorText`); internal level names are constants. Starter Screen/Widget/StyleSheet files live in `Editor/Content/AssetTemplates`.
- Type redirects and level subsystems register by type. Log categories are `Core::LogCategory`; vertex semantics are `RHI::VertexSemantic`.

### 17.10 API correctness (spec 0006)

- `Log` and `Guid` state live behind `new T()` singletons. `Platform::Shell` owns its launches (`Core::AsyncProcess`) and stops them in `Shutdown`.
- Const lookups return const pointers (`MatchConst` overload pairs); property and array accessors have const forms. No `const_cast` remains except `execvp`.
- Long functions (`ThumbnailCache::Generate`, `GenerateFontData`) are split into named steps.
- `AssetDatabase` uses a plain mutex; public members lock, private helpers never do. The threading contract is in `CLAUDE.md`.
- Every `bool` flag parameter is an enum, in the engine and editor. Sibling, root and component positions are `std::optional<size_t>`; indices are `size_t`.
- Errors are `Core::Result<T, E = Core::Error>`. `Core::Error` is `Core::TextError<EngineText>` and `EditorError` is `Core::TextError<EditorText>`; each keeps its id, its arguments as strings and the formatted text. A wrapped failure is formatted into one argument of the outer error.
- Converted: JSON and object deserialization, shader preprocessing and translation, `CreateShader`/`CreatePipeline`, asset importers (`Cook`, `Load`, `CreateAsset`), `CookAsset`, `CookAll` (a list of failures), `LoadAsset`/`Load<T>`/`AssetReference::Load`, UI markup, style sheets and bindings, font generation, project, manifest and game module loading, `SharedLibrary::Load`, command line parsing, `LoadLevel`, `CreateLevelAsset`, `CreateObject`, `Spawn`, `AddComponent`, `Instantiate`, `OpenAsset`, and the editor's build, module promotion, project open and text overrides.
- Asset load errors: `AssetNotSet`, `AssetNotFound`, `AssetWrongType`, `AssetLoadFailed`. `Assets::ReportLoadFailure` logs everything but the first two.
- Verified: tests pass, test renders and the Level View are pixel-identical, cooked bytes are unchanged, the packaged game builds and runs, and editor play mode spawns the player.

### 17.11 Collider (spec 0007)

- One `Physics::Collider` with a reflected `ColliderShape` (Box, Sphere, Mesh). Capsule was dropped; the ArenaDemo player uses a 0.8 × 1.8 × 0.8 box.
- Box and Mesh use `Size` (XYZ; for Mesh a scale starting at 1:1:1). Sphere uses `Diameter`. Switching: to Sphere copies X into Diameter, Sphere to Box fills X=Y=Z, Mesh to Box keeps Size, to Mesh resets Size to 1:1:1.
- Conversion runs in `OnPropertyEdited` (Inspector edits) and `SetShape` (code), never on load. `IsPropertyVisible` hides the other shapes' fields.
- A Mesh collider without a model asks its object for a `Geometry::MeshSource` (implemented by `MeshRenderer`). The gizmo draws the mesh wireframe.
- Old collider classes were removed without migration; the sample files were rewritten by hand to keep their GUIDs.

### 17.12 Scripts (spec 0010)

- Project game code lives in `Scripts/` (was `Source/`). The Assets panel has three roots: Assets, Scripts and Built-in, with Engine Classes under Built-in (`AssetsRoot` in the panel state).
- `ScriptLibrary` (editor service) scans `Scripts/` lazily and pairs `.h`/`.cpp` by stem; ids are hashes of the relative path. File changes in `Scripts/` mark it dirty.
- The existing asset commands (Open, Rename, Delete, Show in Finder, Copy Path) act on scripts when the target is in the Scripts root; Open Header is its own command. Scripts can be moved by drag and drop inside Scripts; folders can be created, renamed and moved.
- Create Script (`CreateScriptDialog`): class name, template (seven `ScriptTemplate`s), folder. Entry points: Assets create menu, Scripts create menu, Level Contents create menu (via the `NewScript` command, Ctrl+Shift+N), File > New Script, and New Script in the Add Component popup. A component created from Add Component is added by `PendingScriptComponent` when the next reload succeeds; a failed compile cancels it.
- Rename rewrites whole-word uses of the class in its two files and appends a `TypeRedirectRegistration` from `ScriptTemplates/TypeRedirect.cpp.in`. Delete moves both files to the Trash (`Platform::Shell::MoveToTrash`, NSFileManager on macOS).
- Reflected classes record their header path (`TypeInfo::GetHeaderPath`, written by the header tool). Engine classes all share the module `BrightBox3D`, so Engine Classes groups them by the first folder of that path; opening one opens the header from `FileSystem::EngineSourceDirectory()` when the engine source is present.

### 17.13 Point and spot light shadows (spec 0012)

- `Light` has Shadow Resolution (Low / Medium / High = maximum tile 512 / 1024 / 2048), Shadow Bias (0.0005) and Shadow Normal Bias (1.5), shown only for point and spot lights that cast shadows. Cast Shadows defaults to on for every type. The directional light keeps the global cascade settings.
- The Camera adds Local Shadows, Local Shadow Atlas Size (4096 or 8192, default 8192 = 256 MB) and Local Shadow Distance (40 m), copied into `RenderView::ShadowSettings`.
- `DrawCollector::CollectLights` gathers a `LocalShadowCandidate` per shadow-casting point/spot light. `LocalShadowPass` (after `ShadowPass`) ranks them with `LocalShadowSelection` (skips lights outside the view or beyond the local distance; score = intensity × range² / max(distance², range²); tile size halves with screen coverage, minimum 128) and places tiles with `ShadowAtlasAllocator` (power-of-two buddy allocator, halving on a full atlas, point lights get all 6 faces or none, at most `MaxLocalShadowViews` = 64 views per frame).
- All views render each frame into one `Depth32Float` atlas: spot = one perspective view (2 × outer angle, capped at 170°), point = six 90° faces. Each FOV is widened by the filter radius (`BRIGHT_LOCAL_SHADOW_FILTER_RADIUS`, 1.5 texels, in `ShaderBindings.h`) so filtering stays inside the tile. Near plane 0.05 m, far plane = range.
- `Lighting.hlsl` samples with a 16-tap Poisson disc, picks the point light face from the major axis, applies normal bias scaled by distance, and fades out over the last 10 % of the local distance. `GpuLight.spotShadow.w` holds the first view index (−1 = none). The Shadows debug view combines directional and local shadows.
- Not cached: every shadowed light re-renders every frame. Windows/DX11 is untested.
