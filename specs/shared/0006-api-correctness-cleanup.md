# Spec 0006: API correctness cleanup

Status: **implemented** (2026-10-18)
Findings taken: 2026-10-13, on the uncommitted working tree

**Line numbers** come from a snapshot taken while another agent was implementing spec 0003. Find each site again by the quoted code, not by its line number alone.

## 1. Purpose

These findings come from a review of the code after specs 0001–0005 were written. None of those specs covers them. They are about how the engine's APIs report errors, keep `const` honest, name their options, and own their threads. Hard-coded strings are in spec 0005, and file splits and duplication are in spec 0004.

## 2. Findings

### R1. No single error-reporting convention

There are three styles in use:

| Style | Where |
|---|---|
| `bool F(…, std::string& error)` (36 declarations) | `UI/Style.h:100` `Parse`; `UI/Document.h:57,58,70,80`; `UI/Binding.h:63,79`; `UI/FontData.h:47`; `Runtime/Project.h:16,19`; `Runtime/GameModule.h:22,23,31`; `Runtime/GameManifest.h:20`; `RHI/ShaderCompiler.h:23,24`; `RHI/Device.h:33,34`; `Rendering/RenderingImporters.h:11,12,19`; `Geometry/ModelImporter.h:11,12`; `Assets/AssetImporter.h:22,23,31,32,35,42,43`; `Assets/AssetDatabase.h:78,79`; `Editor/Source/GameBuilder.h:14`; `HotReload.h:56`; `EditorText.h:333`; `Services/ProjectService.h:17` |
| `F(…, std::string* error = nullptr)` (5) | `Serialization/ObjectSerializer.h:21`; `Serialization/Json.h:15,16`; `Level/LevelSerializer.h:31,35` |
| Log inside, then return `nullptr` / `false` | e.g. `AssetDatabase::LoadAsset` (`AssetDatabase.cpp:562,570`), `Level::CreateObject` (`Level.cpp:141,147`), `LevelObject::AddComponent` (`LevelObject.cpp:203,209`) |

Problems:

- The same failure is logged twice. `AssetDatabase::LoadAsset` logs and returns null, then `LevelStreaming.cpp:103` and `LevelSerializer.cpp:791` log their own message for the same failure.
- With an out-parameter, the caller can't tell from the signature whether the function already logged.
- `std::string& error` forces every caller to declare a throwaway string. `std::string*` with a default lets callers ignore the error silently.
- Errors travel as already-formatted strings. The text now comes from `EngineText` / `EditorText`, but callers can't tell one failure from another without comparing text.

### R2. `const_cast` working around missing const overloads

| Where | Cause |
|---|---|
| `Level/LevelObject.h:87` `GetComponent<T>() const`; `LevelObject.cpp:281` `GetComponent(const TypeInfo&) const`; `LevelObject.cpp:297` `FindComponentByLocalId() const` | `const` lookups return a mutable `LevelObjectComponent*` / `Transform*` |
| `Level/LevelSerializer.cpp:71,210,266,955` | `LevelObject::GetLinkRoot()` (`LevelObject.h:143`) has no `const` overload |
| `Reflection/TypeInfo.h:106`; `Serialization/ObjectSerializer.cpp:277` | `PropertyInfo::access` and `ArrayAccess::element` exist only in a mutable form |

Spec 0002 removed only the `const_cast` in `TypeRegistry::Register`. With these, a `const LevelObject&` can still be written through.

Not findings: `Core/Process.cpp:230` (the `execvp` signature requires it).

### R3. `bool` and sentinel-`int` parameters

The typed-API rule (enums so the IDE shows the options) is broken by flag parameters whose call sites don't say what they mean:

| Declaration | Example call site |
|---|---|
| `LevelObject::SetParent(LevelObject* parent, bool keepWorldTransform = true, int siblingIndex = -1)` (`LevelObject.h:60`) | `UndoCommands.cpp:84` `SetParent(parent, true, location.sibling)`; `ObjectEditService.cpp:263,267`; `Tests/Level/LevelTests.cpp:95` `SetParent(parent, false)` |
| `Transform::Translate(…, bool worldSpace = false)`, `Rotate(…, bool worldSpace = false)` (`Transform.h:36,37`) | Game code |
| `SelectionService::Select(const Guid&, bool additive = false)` (`SelectionService.h:31`) | Editor panels |
| `PhysicsWorld::Raycast(…, RaycastHit&, bool includeTriggers…)` (`PhysicsWorld.h:41`) | Game code |
| `LevelSerializer::SaveLevel(Level&, bool onlyDirty = false)` (`LevelSerializer.h:32`) | `LevelService.cpp:133` `SaveLevel(*m_level, false)` |
| `LevelSerializer::WriteObject(…, bool …)` (`LevelSerializer.h:24`), `ReadObject`, `Instantiate`, `InstantiateNode` (`:25,38,41`) | Serializer internals |
| `TypeRegistry::GetDerived(const TypeInfo&, bool includeAbstract = false)` (`TypeRegistry.h:23`) | Editor menus |
| `Level::FindComponentsOfType(bool includeInactive = false)` (`Level.h:118`); `LevelObject::FindChild(name, bool recursive = true)` (`LevelObject.h:65`) | Game code |
| `DebugDraw::Line/WireBox/Circle/Cone/Frustum(…, bool depthTest = true)` (`DebugDraw.h:20–27`) | Game code |
| `Serialization::WriteJson(const JsonValue&, bool pretty = true)` (`Json.h:17`) | Internal |

Sentinel `int index = -1` meaning "append at the end": `LevelCollection::AddRoot` (`LevelCollection.h:27`), `Level::AttachComponent` (`Level.h:220`), `LevelObject::AttachComponent` (`LevelObject.h:159`), `LevelObject::SetParent` (`:60`). `LevelObject::MoveComponent(component, int index)` (`:71`) and `LevelService::GetCollectionByIndex(int)` (`LevelService.h:34`) take signed indices into `std::vector`.

`ObjectEditService::CreateLight(int lightType)` is already covered by 0004 S5.

### R4. Long functions not split by spec 0004

| Function | Length |
|---|---|
| `ModelImporter::Cook` (`Geometry/ModelImporter.cpp:27`) | ~188 lines: ufbx load, node walk, mesh conversion, material extraction, tangent generation, serialization. 0004 G1 splits `RenderingImporters.cpp` only, and D19 touches only its settings |
| `ThumbnailCache::Generate` (`Editor/Source/Thumbnails.cpp:88`) | ~128 lines: texture path, model/material path with a temporary level, render-target setup, readback |
| `GenerateFontData` (`UI/FontData.cpp:94`) | ~114 lines: FreeType setup, glyph-set building, msdf-atlas packing, bitmap copy |

### R5. Platform `#if` blocks in editor code

`EditorApplication.cpp`'s platform blocks moved into `Platform/Shell`, and the player executable name and `.exe` suffix moved into `Runtime::BuildLayout`. Two editor files still branch on the platform:

- `Editor/Source/HotReload.cpp`: `cmake` / `cmake.exe`, four install paths, `ninja` / `ninja.exe` (`FindCMake`, `FindNinja`).
- `Editor/Source/GameBuilder.cpp`: platform folder name (`PlatformName`).

### R6. Detached threads

`Platform::Shell::Launch` (`Engine/Source/Platform/Shell.cpp`), used by "open in code editor" and "reveal in file browser", runs `std::thread([command] { Core::Process::Run(command); }).detach()`. Nothing owns these threads, and they can still be running while the editor shuts down. `Core::Process` already runs commands asynchronously (`Process.h:68` owns its `std::thread`).

### R7. `AssetDatabase` as a global, and its threading rules

- `Assets::AssetDatabase::Get()` is called from 15 engine files and from the editor (78 call sites in all). Asset loading is a hidden dependency of `Renderer`, `GpuResourceCache`, `ScreenRenderer`, `Document`, `Font`, `LevelSerializer`, `LevelStreaming` and the importers.
- `LevelStreaming.cpp:91` calls `AssetDatabase::Get().Load<LevelCollectionAsset>()` on a `std::async` worker thread. The database is guarded by a `std::recursive_mutex` (`AssetDatabase.h:117`, ~20 locks in `AssetDatabase.cpp`). Nothing documents which members are safe from other threads, and the recursive mutex suggests re-entrant locking that wasn't designed on purpose.
- Other engine-wide singletons holding state: `UI::ScreenRenderer::Get()` (GPU resources, `ScreenRenderer.cpp:18`), `FontLibrary` (`Font.cpp:115`), `LevelObjectLibrary` (`LevelSerializer.cpp:1248`). 0004 D2's `RenderContext` takes over creating and destroying `ScreenRenderer` but keeps it a singleton.

Singletons are the project convention (`CLAUDE.md`), so this is a question, not a defect (Q5).

### R8. Singleton pattern inconsistencies

The convention is `static T& Get()` backed by `static T* x = new T();`. These use function-local statics instead:

- `Core/Log.cpp:16` `static std::mutex mutex;`, `:32` `static LogState state;`
- `Core/Guid.cpp:30–31` `static std::mutex mutex;`, `static std::mt19937_64 engine`

Log is called from worker threads and during static destruction (asserts, module unload). A function-local static object can be destroyed before its last caller, which is exactly what the `new T()` convention avoids.

## 3. Overlaps

| Spec | Overlap |
|---|---|
| 0002 | Removed the registry `const_cast` (R2 covers the rest) |
| 0003 | `EditorText` (R1's typed errors would feed it) |
| 0004 | G1/D19 importers (R4). Already done there: `EngineText` (R1), `Platform::Shell` (R5, R6), `RenderContext` (R7) |

## 4. Open questions (owner decisions needed)

| # | Question | Options |
|---|---|---|
| Q1 | Error convention (R1) | (a) `Core::Result<T>` (value or `Core::Error`). C++20 has no `std::expected`, so it's a small engine type. Functions return it and never log; the caller decides. (b) Keep `bool` + out-parameter, but standardize on `std::string& error` and "never log when returning an error". (c) Keep `Result<T>` and also make `Core::Error` carry a typed code (`EngineText` entry + arguments) instead of a string |
| Q2 | Const fixes (R2) | (a) Add `const` overloads (`const Transform* GetComponent<Transform>() const`, `const LevelObject* GetLinkRoot() const`, a `const` property accessor) and remove every `const_cast` listed. (b) Make the lookups non-const and drop the `const` from callers |
| Q3 | Flag parameters (R3) | (a) An enum per option (`TransformSpace::World`, `SelectionMode::Add`, `ParentMode::KeepWorldTransform`, `TriggerQuery::Include`, `SaveScope::DirtyOnly`, `DepthTest::On`, …). (b) Option structs with designated initializers (`SetParent(p, { .keepWorldTransform = true })`), matching the reflection macro style. (c) Only public game-facing APIs (`Transform`, `Raycast`, `DebugDraw`, `FindComponentsOfType`, `FindChild`, `SetParent`). Editor and serializer internals stay as they are |
| Q4 | Sentinel indices (R3) | (a) `std::optional<size_t>` for "position, or append". (b) Separate overloads (`AddRoot(object)` / `InsertRoot(object, size_t index)`). (c) Leave as is |
| Q5 | `AssetDatabase` global and threading (R7) | (a) Keep the singleton; document the thread-safety contract on `AssetDatabase.h` (which members may be called from workers) and replace the recursive mutex with a plain one plus non-locking private helpers. (b) Pass an `AssetDatabase&` (or an interface) into the systems that use it, and keep `Get()` only for game code. (c) Leave as is |
| Q6 | Singleton storage (R8) | (a) Change `Log` and `Guid` to the `new T()` convention. (b) Leave as is |
| Q7 | Long functions (R4) | (a) Split each into private steps in the same file. (b) Split into new files (`ModelImportSteps`, `ThumbnailRenderer`, `FontAtlasBuilder`). (c) Fold into 0004 phase G |
| Q8 | Platform blocks and detached threads (R5, R6) | (a) `Platform::Shell` gets `FindExecutable(name, candidates)` and `ExecutableFileName(base)`; launching uses `Core::Process` instead of a detached thread. (b) Do it here as its own step |
| Q9 | Order | (a) Before 0004, since error and const changes touch signatures that 0004 then moves. (b) After 0004. (c) R1 and R3 before 0004, the rest after |

## 4a. Owner decisions (2026-10-15)

| # | Decision |
|---|---|
| Q1 | (c) `Core::Result<T>` with a typed `Core::Error` (`EngineText` id + arguments). Functions never log; the caller decides |
| Q2 | (a) Const overloads; remove every `const_cast` except `execvp` |
| Q3 | (a) An enum per flag option, engine and editor |
| Q4 | (a) `std::optional<size_t>` for "position or append"; signed indices become `size_t` |
| Q5 | (a) Keep the singleton; plain mutex plus non-locking private helpers; the thread-safety contract is recorded in `CLAUDE.md` (no code comments) |
| Q6 | (a) `Log` and `Guid` use the `new T()` convention |
| Q7 | (a) Named steps in the same file |
| Q8 | (a) `Platform::Shell` launches through `Core::Process` it owns |
| Q9 | Moot: 0004 and 0005 are done |
| Q10 | Editor errors: `Core::Result<T, E = Core::Error>`; the editor adds `Editor::EditorError` (`EditorText` id + arguments) |
| Q11 | A wrapped lower-level error is formatted into one of the outer error's arguments |
| Q12 | Game-facing log-and-return-null calls (`Load<T>`, `CreateObject`/`Spawn`, `AddComponent<T>`) return a pointer-like `Core::Result` (`operator bool`, `operator->`); the caller logs |

Already resolved before this spec: R5 (constants in `EditorLayout` / `BuildLayout`), `ModelImporter::Cook` (0004 G1).

## 5. TODO (in order; commit and push each)

- [x] **T1 — Singletons (R8).** `Log` and `Guid` state behind `new T()`.
- [x] **T2 — Shell launches (R6).** `Platform::Shell` owns its `Core::Process` launches; no detached threads.
- [x] **T3 — Const (R2).** Const overloads for component lookups, `GetLinkRoot`, property and array element access; remove the `const_cast`s.
- [x] **T4 — Long functions (R4).** `ThumbnailCache::Generate` and `GenerateFontData` split into named steps.
- [x] **T5 — AssetDatabase locking (R7).** Plain mutex, non-locking private helpers, contract in `CLAUDE.md`.
- [x] **T6 — Flag enums (R3).** One enum per bool flag option.
- [x] **T7 — Indices (R3).** `std::optional<size_t>` positions; `size_t` indices.
- [x] **T8 — Result (R1).** `Core::Result<T>` / `Core::Error`; convert the out-parameter and log-and-return-null APIs; callers log once.
- [x] **T9 — Verify.** Tests, screenshots, cooked output, packaged game.
