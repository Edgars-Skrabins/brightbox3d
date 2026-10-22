# BrightBox3D

C++20 3D game engine: an Unreal-style gameplay framework, a Unity-style editor, and C++ hot reload. The authoritative design is `specs/shared/SPEC.md`; §17 there records implementation decisions. Feature specs live next to it in `specs/shared/`; `specs/personal/` is gitignored. The README only describes the engine, its tech stack and how to build and launch it; it never mentions other engines.

## Repository layout

| Path | Contents |
|---|---|
| `Engine/Source/<Module>` | Engine shared library `libBrightBox3D`. Modules: `Core`, `Math`, `Platform`, `RHI` (Metal + DX11), `Reflection`, `Serialization`, `Assets`, `Geometry`, `Level`, `Rendering`, `Physics` (Jolt), `UI`, `Framework`, `Runtime`. Each module is its own CMake object library |
| `Engine/Content` | Runtime content shipped with games: HLSL shaders, fonts. `Shaders/ShaderBindings.h` holds every shader slot number for both HLSL and C++. `Shaders/ThirdParty` is fetched at configure time and gitignored |
| `Editor/Source` | Editor. Built as the static lib `BrightEditorCore` plus the `BrightEditor` exe. `Panels/` holds dockable windows, `Services/` the editor services owned by `EditorContext`, `Commands/` the command system, `ImGui/` the backends |
| `Editor/Content` | Editor-only content: `AssetTemplates/` and `ScriptTemplates/` (starter files), and `Fonts/ThirdParty/Lucide` (icon font, fetched at configure time and gitignored) |
| `Player/Source` | `BrightPlayer`, the standalone runtime used by packaged games |
| `Tools/BrightHeaderTool` | Reflection code generator. It writes a `.gen.cpp` per reflected header |
| `Tests/<Area>` | GoogleTest suite `BrightTests`. `System/` builds and runs a full packaged game |
| `Samples/ArenaDemo` | Sample game project, used by the system tests |
| `cmake/` | `Dependencies.cmake` (FetchContent), `BrightReflection.cmake`, `BrightGame.cmake` (game module builds), `ModuleDependencies.cmake` (allowed module includes) and `CheckModuleDependencies.cmake` (the build-time check) |
| `specs/` | `shared/`: the engine design (`SPEC.md`) and numbered feature specs, committed. `personal/`: private notes and reference images, gitignored |

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

Editor automation flags, useful for visual checks: `--select <name>`, `--play`, `--build`, `--frames N`, `--screenshot <png>`, `--play-screenshot <png>`, `--create-project`, `--add-panel <EditorWindow id>` (for example `--add-panel Terminal`). To check UI work, take a screenshot and look at it:

```bash
./build/bin/BrightEditor Samples/ArenaDemo --select Crate --frames 60 --screenshot /tmp/shot.png
```

## Architecture

- **Object model:** `Reflection::EngineObject` is the root of every reflected class and declares the lifecycle callbacks once. `Level::LevelObject` lives in the world and owns `Level::LevelObjectComponent`s. `Framework::PlayableLevelObject` is possessed by a `Framework::PlayerController`. `GameModeBase`, `GameStateBase`, `PlayerController`, `Level::LevelBehavior` and level subsystems are Level-owned EngineObjects, not world objects.
- **Gameplay settings** live per Level (`gameMode` and `levelBehavior` in the Level's settings), read through `Framework::LevelGameplaySettings`.
- **Assets:** `LevelObjectTemplate` (`.template`) can't be placed; `LevelObjectVariant` (`.variant`) is placed or spawned (`Level::Spawn`) and may be based on a Template.
- **AssetDatabase threading:** worker threads may only call `LoadAsset` / `Load<T>`, `FindLoaded` and `GetImporter`. Everything else (`Refresh`, file operations, `Open`/`Close`, and the `AssetRecord` pointers `FindRecord` returns) is main-thread only. Importers are registered at startup, before any worker runs. The database uses a plain mutex: public members lock, private helpers never do, so a locked member must call the private helper (`FindRecordUnlocked`, `ImporterFor`), never another public member.
- **Scripts:** game code lives in `<Project>/Scripts/` (`ProjectLayout::ScriptsFolder`), separate from `Assets/`, and is compiled by `HotReload`. The editor's `ScriptLibrary` (on `EditorContext`) pairs `Name.h`/`Name.cpp` into one script; `Scripts/ScriptFiles` does create, rename (adds a `TypeRedirectRegistration`), move and delete (to the Trash). New scripts come from `ScriptTemplate` entries, each with `.h.in`/`.cpp.in` files in `Editor/Content/ScriptTemplates/` using the `@CLASS@` placeholder.
- **Hot reload:** engine-owned state lives in `EngineObjectState`, `LevelObjectState` and `ComponentState`, moved whole by `ObjectReplacer`. Raw pointers between objects survive a reload only through `CarryReferences`.
- **Module boundaries:** `cmake/ModuleDependencies.cmake` is the single source of truth; the build fails on a forbidden `#include`. Add a module there before using it.
- **Renamed classes:** add a `Reflection::TypeRedirectRegistration<NewType> s_redirect("OldName");` so saved files with the old name still load.
- **Level subsystems** register by type: `Level::LevelSubsystemRegistry::Register<MySubsystem>()`.

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
  - Typed designated initializers, fields in declaration order: `BrightClass(.category = Categories::Gameplay, .icon = Icon::Flag, .abstract = true)`, `BrightProperty(.edit = Edit::Anywhere, .category = Categories::Physics)`, `BrightUIProperty(.access = UIAccess::Read)`, `BrightEnum()`, plus `BrightBody()` in the class.
  - Never pass strings as metadata. Categories are `constexpr Reflection::Category` values and display names are `constexpr Reflection::DisplayName` values (`.displayName = DisplayNames::Position`), both in each module's `Categories.h`. Without a display name, the member name is prettified.
  - CMake generates `.gen.cpp` files; never edit them.
- **No strings in typed APIs:** use enums and types so the IDE lists what's possible.
- **No flag parameters:** a `bool` option is an enum (`TransformSpace`, `SelectionMode`, `FieldAccess`, ...). A position that may mean "append" is `std::optional<size_t>`; indices are `size_t`.
- **Errors:**
  - A function that can fail returns `Core::Result<T>` (`Core/Result.h`): a value or a typed `Core::Error` (`Core/Error.h`, an `EngineText` id plus its arguments). Editor code returns `Core::Result<T, EditorError>` (`EditorText` ids). Never use `std::string& error` out-parameters.
  - A function never logs its own failure. The caller decides, logging `GetError().GetText()` once or wrapping it as an argument of its own error.
  - Asset loads report with `Assets::ReportLoadFailure`, which stays silent when the asset is unset or missing (`Assets::IsAbsentAsset`).
- **Editor text:** no user-visible string literal in editor code. Add an entry to `Editor/Source/EditorText.h/.cpp` and use `Text(EditorText::X)` or `Text(EditorText::X, args...)` (`std::format` placeholders). ImGui `##` IDs and persistence keys are the only other strings. Window titles come from `EditorWindow` (`WindowTitle`).
- **Commands:** every toolbar button, menu item and shortcut is a command. Add a `CommandId`, register it in `Editor/Source/EditorCommands.cpp`, and draw it with `CommandButton`, `CommandMenuItem` or `DrawToolbar`. Panels dispatch their own scope with `CommandRegistry::Get().DispatchShortcuts(scope)`.
- **Editor colors:** never hard-code a color. Add an entry to `EditorColor` / `ColorInfos` in `Editor/Source/EditorPreferences.*` and read it with `EditorColorVec4` / `EditorColorU32`. Theme-wide ImGui colors are derived in `Editor/Source/Theme.cpp`.
- **Icons:** use `Icon::` (every Lucide icon is generated into `Core/Icon.h`) and draw it with `IconGlyph(Icon::X)` or `IconLabel`. Icon-only buttons use `IconButton(Icon::X, EditorText::Tooltip)`, or `CommandButton(id, 0, CommandButtonStyle::IconOnly)` for commands, so the glyph is centered. Classes declare theirs with `BrightClass(.icon = ...)`.
- **Selection highlight:** wrap list and tree panels in `PushSelectionColors()` / `PopSelectionColors()`.
- **Inspector hooks:** an object hides properties that don't apply by overriding `EngineObject::IsPropertyVisible` (compare `property.GetValuePointer(this)` with the member, no names). `OnPropertyEdited(property)` runs only for user edits in the Inspector, before `OnPropertiesChanged`; loads and undo call only `OnPropertiesChanged`.
- **Panels:** dockable panels derive from `EditorPanel` (`Editor/Source/Panels/EditorPanel.h`) and open their window with `BeginWindow()`. Any panel can exist more than once, so per-instance state lives in the panel, never in a service. Look panels up through `EditorApplication` (`GetActiveLevelView()`, `GetAssets()`, ...), which returns `nullptr` when none is open. Panels save their own state with `SaveState` / `LoadState` into `EditorPanels.json`.
- **Logging:** engine and editor code log through `Core::Log` (tagged `LogSource::Engine`). Game code logs through `BrightBox3D::Debug::Log` / `LogWarning` / `LogError` (`Core/Debug.h`, tagged `LogSource::Game` with the caller's file and line).

## Code quality

- **One responsibility per file.**
  - Split a class that does several jobs.
  - Stateful pieces become classes owned by their parent. Stateless helpers become namespaces of free functions.
  - A large class may spread its member definitions over files named by responsibility (`Level.cpp`, `LevelObjects.cpp`, `LevelPlay.cpp`).
  - A long function is broken into named steps.
- **No duplication.** Before writing a helper, check the shared utilities below.
- **No hard-coded strings.**
  - User-visible engine text (logs, errors, default names) goes in `Engine/Source/Core/EngineText.h/.cpp` and is read with `Core::Text(Core::EngineText::X, args...)`.
  - Editor UI text goes in `EditorText`.
  - Text that names an engine type uses a `{}` placeholder filled with the type's name: `TypeName<T>()` / `AssetTypeLabel<Type>()` (`Editor/Source/Labels.h`) or `Assets::AssetTypeName(type)`.
  - Paths and file names live in layouts: `Core::EngineLayout` (engine content), `Editor::EditorLayout` (editor content, user settings files, build tools), `Runtime::ProjectLayout` and `Runtime::BuildLayout`.
  - Per-asset-type data lives in the `AssetType` table (`AssetTypeName`, extensions, `NewAssetName`, `IsCreatableAssetType`, `MatchedExtension`); editor-only per-type data in `AssetTypeStyle`.
  - Starter content for new assets lives in `Editor/Content/AssetTemplates/`.
  - Identifiers used in more than one place are one named constant (popup IDs) or an enum (`Core::LogCategory`, `RHI::VertexSemantic`).
  - File format keys live in each module's schema header (`LevelFileSchema`, `AssetFileSchema`).
  - Fixed sets of options are enums, never strings.
- **No magic numbers.** Name constants. Editor sizes are `EditorMetric` entries in `Editor/Source/Theme.h`, read with `ScaledMetric(EditorMetric::X)`.
- **Shared utilities:**

| Utility | Where | Use for |
|---|---|---|
| `Core::StringUtils` | `Engine/Source/Core/StringUtils.h` | Split, trim, case, prefix and suffix checks, `PrettifyName`, `Widen` (Windows) |
| `ColorSpace` | `Engine/Source/Math/ColorSpace.h` | sRGB to linear conversions |
| `MipGenerator` | `Engine/Source/Rendering/MipGenerator.h` | Box-filter mip chains for 8-bit and float pixels |
| `AssetDatabase::IsIgnoredPath`, `ResolveReference` | `Engine/Source/Assets/AssetDatabase.h` | Which files are assets, and resolving asset references |
| Reflected import settings | `TextureImportSettings`, `ModelImportSettings` | Importer options, drawn by the Inspector like any object |
| `JoltConversions`, `MetalConversions`, `DX11Conversions` | `Physics/`, `RHI/Metal/`, `RHI/DX11/` | Engine types to backend types |
| `DragDrop` | `Editor/Source/DragDrop.h` | Typed drag and drop payloads (`DragDropPayload`) |
| `InlineRename` | `Editor/Source/Widgets/InlineRename.h` | Rename in place in lists and trees |
| `CreateMenuRegistry` | `Editor/Source/CreateMenuRegistry.h` | Create menu entries. Built-ins go in `EditorCreateMenu.cpp`; reflected classes appear automatically |
| `ValueDrawers` | `Editor/Source/ValueDrawers.h` | Drawing one property value per `PropertyKind` |
| `AssetTypeStyle` | `Editor/Source/Panels/Assets/AssetTypeStyle.h` | Icon and color per `Assets::AssetType`, and the icon-plus-name row label |
| `AssetActions` | `Editor/Source/Panels/Assets/AssetActions.h` | Asset operations shared by the Assets layouts and commands (open, rename, move by drop, copy path) |
| `IconButton`, `DrawCenteredIcon` | `Editor/Source/Theme.h` | Square icon buttons and centered glyphs at any size |
| `ScriptLibrary`, `ScriptFiles` | `Editor/Source/Services/ScriptLibrary.h`, `Editor/Source/Scripts/ScriptFiles.h` | Finding scripts in `Scripts/`, and script and script-folder file operations |
| `Platform::Shell::MoveToTrash` | `Engine/Source/Platform/Shell.h` | Deleting files recoverably |
| `TypeInfo::GetHeaderPath`, `FileSystem::EngineSourceDirectory` | `Engine/Source/Reflection/TypeInfo.h`, `Engine/Source/Core/FileSystem.h` | Locating a reflected class's header |
| `TestUtilities` | `Tests/Common/TestUtilities.h` | `MakeTempDirectory`, `TestOutputPath` (reads `BRIGHT_TEST_OUTPUT`), `CreateTestRenderContext`, `Succeeded(result)` (asserts a `Core::Result` and prints its error) |

## Workflow rules

- Run `BrightTests` before presenting work. For editor UI changes, also check a screenshot.
