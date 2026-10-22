# Spec 0005: Hard-coded string cleanup

Status: **implemented** (2026-10-15)
Inventory taken: 2026-10-13, on the uncommitted working tree

**Line numbers** come from a snapshot taken before specs 0003 and 0004 Phases A and B, which changed most of these files. Find each site again by its quoted literal, not by its line number alone. Rows fixed by that work have been removed.

## 1. Purpose

This spec lists every hard-coded string in `Engine/Source`, `Editor/Source`, `Player/Source` and `Tools/BrightHeaderTool` that names something the code already knows as a type, enum, constant or table entry, or that is repeated in several places. The example that started it:

```cpp
if (ImGui::MenuItem("LevelObjectTemplate"))
```

The string repeats `Assets::AssetType::LevelObjectTemplate`, whose name already lives in `AssetType.cpp:22`. If the type is renamed, the menu goes stale without a compile error.

Specs 0003 and 0004 already plan fixes for much of this. Every row below says which spec covers it:

| Tag | Meaning |
|---|---|
| **0003 T8a** | Editor UI text → `EditorText` table |
| **0003 T8b** | Shortcut display strings → commands |
| **0004 Dn / Sn** | The 0004 finding with that ID |
| **NEW** | Not covered by any spec yet. Needs an owner decision in §5 |
| **PARTIAL** | An existing spec touches it but leaves the site listed here open |

## 2. Not findings (left as they are)

These strings were looked at and are deliberately not listed:

- ImGui `##id` / `###id` suffixes, `PushID` keys and child or table IDs that are used only once (0003 §3.7 keeps them inline).
- `printf` / `snprintf` / ImGui format strings (`"%.3f"`, `"%s"`, `"%02d:%02d:%02d"`).
- `static_assert` messages (`PropertyTraits.h:107,113,119,130,131`, `ClassReference.h:21`).
- C++ text emitted by the header tool (`Generator.cpp:22–92`) and its tokenizer and parser punctuation (`Parser.cpp` `"("`, `"{"`, `"::"`, …).
- Regex patterns for compiler output (`HotReload.cpp:324–325`).
- Persistence keys that are already the single table entry (`EditorPreferences.cpp:20–59` column 1, `EditorTextInfo::key`).
- Old class names in `TypeRedirectRegistration` (`GameModeBase.cpp:14` `"GameMode"`, `LevelObject.cpp:14` `"LevelObjectInstance"`). They have to stay strings, because the old types no longer exist. The new-name half is listed in A5.
- `Core/Guid.cpp:70` hex digits, `Core/StringUtils.cpp:76` `"m_"` prefix rule.
- `Samples/ArenaDemo` game code (`ArenaPlayer.h:29`, `ArenaLevelBehavior.cpp:7`) and `ArenaCategories.h`. That is the game developer's own code.
- `Tests/` (see Q9).

## 3. Inventory

### A. Engine type names written as strings — the original example

| ID | Where | String(s) | Already known as | Tag |
|---|---|---|---|---|
| A1 | `Editor/Source/Panels/AssetsPanel.cpp:177,184,189,199,203,211,217,223` | Create menu: `"Folder"`, `"Level"`, `"LevelCollection"`, `"LevelObjectTemplate"`, `"Material"`, `"UI Screen"`, `"UI Widget"`, `"UI StyleSheet"` | `AssetTypeName(AssetType::…)` in `AssetType.cpp:17–29` | **PARTIAL** — 0004 S5 says its registry "feeds 0001's Assets Create menu", 0003 T8a would copy the words into `EditorText`. Neither says the label must come from the asset type. Q1 |
| A3 | `Editor/Source/Panels/LevelContentsPanel.cpp:105,225,229,257,261`; `InspectorPanel.cpp:474,605,616,716,771`; `PropertyDrawer.cpp:371,375,449,453`; `AssetCreationService.cpp:51,54,100`; `EditorPreferences.cpp:45,51,52,53` | UI text with a type name built in: `"Empty LevelObject"`, `"Select LevelObject Asset"`, `"Open LevelObject"`, `"Create LevelObjectVariant"`, `"Create LevelObjectTemplate"`, `"Unlink or open the LevelObject…"`, `"Templates are only used to create LevelObjects…"`, `"LevelCollection"`, `"LevelCollections"`, `"Revert to LevelObject"`, `"Apply to LevelObject"`, `"Created LevelObjectVariant '…'"`, color labels `"PlayerStart"`, `"LevelCollection"`, `"LevelObject"`, `"LevelObject Template"` | `TypeInfo::GetName()` / `AssetTypeName` | **PARTIAL** — 0003 T8a moves them into `EditorText`. The type name would still be typed out by hand inside the English text. Q1 |
| A4 | `InspectorPanel.cpp:352` `"Template"` / `"Variant"`; `LevelContentsPanel.cpp:479` `"Collection"`, `:563` `"Level"` row tags | Short type labels | `AssetType` | **PARTIAL** — 0003 T8a. Q1 |
| A5 | `Engine/Source/Framework/GameModeBase.cpp:14` `"GameModeBase"`; `Level/LevelObject.cpp:14` `"LevelObject"` | New-name half of a type redirect | `GameModeBase::StaticType().GetName()` | **NEW** — Q7 |
| A6 | `Engine/Source/Reflection/EngineObject.cpp:9` | `TypeInfo type("EngineObject", …)` written by hand | Only root type without a `.gen.cpp` | **NEW** (minor) — Q7 |
| A8 | `Engine/Source/Level/LevelSerializer.cpp:27` | `MissingLevelObjectType = "$MissingLevelObject"` | Local constant; check whether `MissingComponent` uses a matching marker | **NEW** (minor) — Q2 |

### B. Asset type identity outside the `AssetType` table

| ID | Where | String(s) | Tag |
|---|---|---|---|
| B6 | `Editor/Source/Panels/AssetsPanel.cpp:66–90` | Badge codes per type: `"DIR"`, `"LVL"`, `"COL"`, `"OBJ"`, `"TPL"`, `"MAT"`, `"FBX"`, `"TEX"`, `"SCR"`, `"WGT"`, `"CSS"`, `"FNT"`, `"?"` in a switch | **NEW** — Q3 |
| B7 | `Editor/Source/EditorPreferences.cpp:49–59` | Per-type color entries `AssetFolder` … `AssetOther`, plus the type-to-color mapping in the Assets panel | **NEW** (minor) — Q3 |

### C. File extensions and file-name patterns

| ID | Where | String(s) | Tag |
|---|---|---|---|
| C3 | `EditorApplication.cpp:826` `"Levels/" + name + ".level"`; `Services/LevelService.cpp:14` `"Levels/Main.level"` | Level folder and extension typed out | **NEW** — Q3, Q5 |
| C4 | `Rendering/BuiltinAssets.cpp:274–280` | `"Builtin/Cube.model"` … `"Builtin/Default.material"`. `.model` isn't in the `AssetType` table at all (`Model` maps to `.fbx`) | **NEW** — Q3 |
| C5 | `Assets/AssetDatabase.cpp:17` `MetaExtension = ".meta"`, `:31` `".tmp"`, `:144` `".bin"`; `Runtime/LevelObjectAssetMigration.cpp:14–15` `".levelobject"`, a second `MetaExtension = ".meta"`; `Core/FileSystem.cpp:56` `".tmp"`; `EditorApplication.cpp:341` `".meta"` / `".tmp"` | Meta, temp and cooked extensions defined or typed out four times | **PARTIAL** — 0004 D20 covers `AssetDatabase` vs `EditorApplication`. The migration copy and `FileSystem.cpp` are not listed |
| C6 | `Editor/Source/HotReload.cpp:141` (`.cpp .h`) vs `:216` (`.cpp .h .hpp .inl`) | Two different "game source file" extension lists | **NEW** — Q5 |
| C7 | `GameBuilder.cpp:81` `".exe"`; `Core/SharedLibrary.cpp:66–70` `".dll"`, `"lib….dylib"`, `"lib….so"` | Platform binary naming | **NEW** (minor) — Q5 |
| C9 | `AssetsPanel.cpp:518` | Rename keeps "everything after the first `.`" as the extension, instead of asking `AssetType` for it | **NEW** — Q3 |

### D. Serialization keys (JSON)

| ID | Where | Keys | Tag |
|---|---|---|---|
| D6 | `Runtime/LevelObjectAssetMigration.cpp:16–17` `AssetKindKey = "$asset"`, `MetaTypeKey = "type"`; `Runtime/GameplayMigration.cpp:12` `LevelSettingsKey = "settings"` vs `Framework/LevelGameplaySettings.cpp:15` `SettingsKey = "settings"` | Private duplicate key constants | **NEW** — Q2 |
| D7 | `Framework/LevelGameplaySettings.cpp:12–16` | `gameMode`, `levelBehavior`, `class`, `settings`, `overrideGameMode` (already local constants) | 0004 S12, if the schema includes Level settings. Q2 |
| D10 | `Rendering/RenderingImporters.cpp:158–171`; `Geometry/ModelImporter.cpp:23–30`; `Editor/Source/Panels/InspectorPanel.cpp:528` | Import settings `srgb`, `normalMap`, `generateMips`, `filter`, `wrap`, `scale`, `generateNormals`, with `"Trilinear"` / `"Repeat"` as enum names in strings. The Inspector special-cases `"filter"` and `"wrap"` | 0004 S3, D19 |

### E. Reflected member names compared as strings

| ID | Where | String(s) | Tag |
|---|---|---|---|
| E1 | `Editor/Source/Panels/InspectorPanel.cpp:231–241` | `display.name == "m_localPosition"` → `"Position"`, `"m_localRotation"` → `"Rotation"`, `"m_localScale"` → `"Scale"` | **NEW** — Q4 |

### F. Folder and file names (project, engine, editor, build)

| ID | Where | String(s) | Tag |
|---|---|---|---|
| F6 | `Core/FileSystem.cpp:122–158`; `Editor/Source/Theme.cpp:31–37` | `"EngineContent"`, `"Engine"/"Content"` (×5); `"EditorContent"`, `"Editor"/"Content"` (×4) | **PARTIAL** — 0004 D5 names `EngineContent` for the build only |
| F7 | `EditorApplication.cpp:82`; `Player/Source/Main.cpp:126` | `EngineContentDirectory() / "Shaders"` | **NEW** — Q5 |
| F8 | `UI/Font.cpp:125`; `Editor/Source/Theme.cpp:199` | `"Fonts" / "Roboto-Medium.ttf"`; `"Fonts" / "ThirdParty" / "Lucide" / "lucide.ttf"` | **NEW** — Q5 |
| F9 | `EditorApplication.cpp:56,92,96,141,259,279` | `".brightbox3d"`, `"UserSettings"`, `"EditorLayout.ini"`, `"Preferences.json"`, `"BrightBox3DProjects"`, `"RecentProjects.json"` (×2) | **PARTIAL** — 0004 D17 adds `UserDirectory()`. The file names are not covered |
| F11 | `UI/Document.cpp:237`; `UI/Font.cpp:173`; `Assets/AssetDatabase.cpp:324` | `"Assets/"` path prefix (×3) | **NEW** — Q5 |
| F12 | `AssetsPanel.cpp:362,365,382,419`; `BuiltinAssets.cpp:274–280` | `"Builtin"` virtual folder (×5 in the panel, ×7 as the `"Builtin/"` prefix) | **NEW** — Q3 |

### G. Engine and product names

| ID | Where | String(s) | Tag |
|---|---|---|---|
| G2 | `ImGui/ImGuiRenderer.cpp:16` `"BrightBox3D RHI"`; `ImGui/ImGuiPlatform.cpp:111` `"BrightBox3D Window"`; `Core/FileWatcherApple.cpp:77` `"BrightBox3D.FileWatcher"` | Product name inside backend and queue names | **NEW** (minor) — 0004 D9 should list them |

### H. Default object, asset and level names

| ID | Where | String(s) | Tag |
|---|---|---|---|
| H1 | `Services/LevelService.cpp:105`; `Services/ObjectEditService.cpp:137`; `LevelContentsPanel.cpp:135,139,143` | `"Directional Light"`, `"Point Light"`, `"Spot Light"` — once as menu text, once as object names, indexed by a magic `int` | 0004 S5 (`CreateLight(LightType)`). Names: Q6 |
| H2 | `LevelContentsPanel.cpp:117–122` vs `Rendering/BuiltinAssets.cpp:81,121,152,160,214,267` | Primitive names `"Cube"`, `"Sphere"`, `"Capsule"`, `"Cylinder"`, `"Plane"`, `"Quad"` defined twice | **PARTIAL** — 0004 S5 rewrites the menu; the duplicate with `BuiltinAssets` isn't mentioned. Q6 |
| H3 | `Level/Level.cpp:132`; `LevelSerializer.cpp:896,908` | Default collection `"Main"` (×3) | **NEW** — Q6 |
| H4 | `LevelSerializer.cpp:397,829` | `"Missing LevelObject"`, `"Missing Collection"` | 0004 S9 / Q6 |
| H5 | `Services/LevelService.cpp:109` `"Ground"`; `ObjectEditService.cpp:99` `"Node"`; `Framework/PlayerController.cpp:104` `" Camera"`; `InspectorPanel.cpp:780` `"Collection N"`; `AssetCreationService.cpp:66` `"New Template"`, `:96` `" Variant"`; `AssetsPanel.cpp:179,186,195,208,213,219,225` `"New Folder"`, `"New Level"`, `"New Collection"` (×2), `"New Material"`, `"New Screen"`, `"New Widget"`, `"New Style"`; `EditorApplication.cpp:509` `"NewLevel"`; `EditorApplication.h:105` `"MyGame"` | Default names given to new things | **NEW** — Q6 |
| H6 | `Runtime/GameManifest.h:15`, `GameManifest.cpp:16`, `Runtime/Project.h:45`, `Project.cpp:24` | Default game / module name `"Game"` (×4) | **NEW** — Q6 |
| H7 | `Rendering/BuiltinAssets.cpp:65`; `Geometry/ModelImporter.cpp:93,112` | `"Default"` material (×2), `"Mesh"` | **NEW** — Q6 |
| H8 | `Level/Level.h:41` `"Level"`; `Editor/Source/Thumbnails.cpp:130` `"Thumbnail"`; `LevelSerializer.cpp:1041` `"AssetCopy"`, `:1262` `"Prototype"` | Internal level names | **NEW** (minor) — Q6 |

### I. Identifiers used in more than one place

| ID | Where | String(s) | Tag |
|---|---|---|---|
| I1 | `AssetsPanel.cpp:317,462,468,558`; `LevelContentsPanel.cpp:395,401,417,486`; `PropertyDrawer.cpp:178,320`; `LevelViewPanel.cpp:434` | Drag-and-drop payloads `"BRIGHT_ASSET"`, `"BRIGHT_OBJECT"` | 0004 D11 |
| I2 | `InspectorPanel.cpp:269/271` `"AddComponentPopup"`; `AssetsPanel.cpp:372/374` `"CreateAsset"`, `:577/579` `"Delete Asset?"`; `EditorApplication.cpp:814/817` `"New Level"`; `LevelContentsPanel.cpp:539/541` `"CreatePopup"`; `PreferencesPanel.cpp:94/97` `"Reset All Colors?"` | Popup IDs written twice, once for `OpenPopup` and once for `BeginPopup` / `BeginPopupModal`. A typo silently breaks the popup. The modal ones are also visible titles | **NEW** — Q8 |
| I4 | `Level/LevelStreaming.cpp:191` `"Streaming"`; `Physics/PhysicsWorld.cpp:847` `"Physics"` | `LevelSubsystemRegistry::Register(name, …)` names | **NEW** — Q7 |
| I5 | `HotReload.cpp:243,319,340,346,351,370` `"Compiler"`; `Core/Assert.cpp:11` `"Assert"` | Log category strings | **NEW** — Q8 (spec 0001 adds a `LogSource` enum; check whether it replaces these) |
| I7 | `Tools/BrightHeaderTool/Parser.cpp:151,157,160,166,169,172,183,426,428,430` | Macro names `"BrightClass"`, `"BrightEnum"`, `"BrightBody"`, `"BrightProperty"`, `"BrightUIProperty"` repeated 10× | **NEW** (minor) — Q7 |

### J. Rendering names

| ID | Where | String(s) | Tag |
|---|---|---|---|
| J3 | `Renderer.cpp:159–162,172–173`; `ImGuiRenderer.cpp:87–89`; `ScreenRenderer.cpp:155–160` | Vertex semantics `"POSITION"`, `"NORMAL"`, `"TANGENT"`, `"TEXCOORD"`, `"COLOR"` | **NEW** — Q8 (they must match the HLSL; a shared enum or constants in `ShaderBindings.h` is one option) |
| J5 | `Editor/Source/Panels/LevelViewPanel.cpp:156` | `{ "Lit", "Albedo", "Normals", "Metal/Rough", "Shadows", "AO" }` beside `Rendering::RenderDebugMode` (`RenderView.h:64`), cast through `int` | **PARTIAL** — 0003 T8a moves the words. Q4: derive from a reflected enum instead? |
| J6 | `RHI/DX11/DX11Device.cpp:936` | `"vs_5_0"` / `"ps_5_0"` | **NEW** (minor) |
| J7 | `Renderer.cpp:290–297,579,640,697,704,727–745,922,1044,1124,1192,1201,1212`; `GpuResourceCache.cpp:11–13,22,54,55`; `Thumbnails.cpp:101,123`; `ImGuiRenderer.cpp:41,72`; `LevelViewPanel.cpp:511`; `UI/Font.cpp:49`; `ScreenRenderer.cpp:181`; `DX11Device.cpp:332`; `MetalDevice.mm:350` | GPU debug names | **NEW** — Q8 (probably keep as is) |

### K. Markup and CSS keywords (format boundary)

| ID | Where | String(s) | Tag |
|---|---|---|---|
| K4 | `UI/Binding.cpp:17` | `BindingRootNames` — already a table with a `static_assert` against `BindingRoot::Count` | Already fine |

### L. External tools, platform paths, environment

| ID | Where | String(s) | Tag |
|---|---|---|---|
| L2 | `HotReload.cpp:21–23,60–80,95–97,148–161,306–314,338,344` | `"Ninja"`, `CMAKE_GENERATOR:INTERNAL=`, `getenv("PATH")`, `cmake` / `cmake.exe` and four install paths, `ninja` / `ninja.exe`, CMake arguments, `"RelWithDebInfo"` (×2), generated CMakeLists text, `"warning"` | **PARTIAL** — 0004 S8 covers the generated CMakeLists. The tool discovery and arguments are not covered. Q5 |
| L3 | `Editor/Source/Theme.cpp:173–177` | Fallback system font paths (macOS, Windows, Linux) | **NEW** — Q5 |
| L4 | `GameBuilder.cpp:17–21` | Platform folder names `"Windows"`, `"macOS"`, `"Linux"` | **NEW** (minor) — Q5 |
| L5 | `Core/FileSystem.cpp:108` | `/proc/self/exe` | Fine (platform API) |

### M. Command-line flags

| ID | Where | String(s) | Tag |
|---|---|---|---|
| M3 | `Tools/BrightHeaderTool/Main.cpp:58,71–79` | `--header`, `--output`, `--include` and the usage line | **NEW** (minor) — 0004 D4's `Core::CommandLine` could be reused; the tool doesn't link the engine. Q7 |

### Q. Default file content

| ID | Where | Content | Tag |
|---|---|---|---|
| Q-1 | `Editor/Source/Panels/AssetsPanel.cpp:214,220,226` | Starter XML for a new Screen and Widget, starter CSS for a new StyleSheet | **NEW** — 0004 S8's template files could hold them. Q6 |

## 4. Summary of gaps

Not covered by 0003 or 0004:

1. **Type names typed into editor UI text and menu labels** (A1, A3, A4). They are `EditorText` entries now, but the English text still spells the type name by hand. Engine messages already format the name from `StaticType().GetName()` / `AssetTypeName`.
2. **Private duplicate key constants in the migrations** (D6) and the missing-type marker (A8).
3. **Per-asset-type editor data** (B6, B7, C4, C9, F12): badge codes (now `EditorText` entries, still a switch outside the `AssetType` table), the `.model` extension for built-ins, the `Builtin` folder and rename extension handling sit outside the `AssetType` table.
4. **Reflected member names in the Inspector** (E1).
5. **Paths the layouts don't name yet** (C5, C6, F6–F9, F11, L2–L4): shader and font folders, editor settings file names, the `Assets/` prefix, tool discovery, fallback fonts.
6. **Default names** (H1–H8) and **starter file content** (Q-1).
7. **Paired identifiers** (I2 popup IDs, I4 subsystem names, I5 log categories, I7 macro names, J3 vertex semantics).
8. **Small ones**: A5, A6, G2, J6, M3.

## 5. Open questions (owner decisions needed)

| # | Question | Options |
|---|---|---|
| Q1 | How should text that names an engine type get that name? (A1, A3, A4) | (a) `EditorText` / `EngineText` entries with a `{}` placeholder, formatted with `StaticType().GetName()` or `AssetTypeName(…)`. (b) A display-name field in class metadata (`BrightClass(.displayName = …)`) and the `AssetType` table, used for every label. (c) Leave as plain English in the text tables (0003 as written) |
| Q2 | Where do the `$asset` value and empty-document builders go? (D6, A8) | (a) Fold into 0004 S12's `LevelFileSchema` / `AssetFileSchema`, plus one `CreateEmptyCollectionDocument()`. (b) A separate small step in this spec, before 0004 runs |
| Q3 | Should the `AssetType` table hold all per-type data? (B6, B7, C4, C9, F12) | (a) Yes: name, extensions, badge code, default "New …" name, creatable flag. The editor-only parts (badge, color) go in an editor-side table indexed by `AssetType`. (b) Everything in one engine table. (c) Leave the editor badges as they are |
| Q4 | How do property and enum display names work? (E1, J5) | (a) Add `.displayName` to `PropertyMetadata` (0003 left it out of scope) and rename Transform's displayed properties through it. (b) Rename the members so the auto-derived name is right (`m_position`). (c) Keep a typed per-member override table in the editor |
| Q5 | Where do the folder and file names 0004 D5/D17 don't name go? (C3, C5–C7, F6–F9, F11, L2–L4) | (a) Extend 0004's `ProjectLayout` / `BuildLayout` and add `EngineLayout` (content, shaders, fonts) and `EditorLayout` (user settings files). (b) Named constants in each owning module |
| Q6 | Are default names text or data? (H1–H8, Q-1) | (a) Text: `EditorText` / `EngineText` entries, so they can be translated. (b) Data: named constants beside the thing they name (e.g. `Level::DefaultCollectionName`). (c) Split: user-visible defaults are text, internal level names (H8) are constants |
| Q7 | The small identifier sites (A5, A6, I4, I7, M3): fix them here or skip them? | (a) Fix: the redirect takes the new type as a template argument; subsystems register by type and the name comes from reflection or a typed enum; the header tool uses named constants for macro names. (b) Skip: they're each a single, local use |
| Q8 | Popup IDs, log categories, vertex semantics and GPU debug names (I2, I5, J3, J7)? | (a) Popup IDs become one constant per popup (or a helper that owns the open/begin pair); log categories become an enum (check spec 0001's `LogSource`); vertex semantics become constants shared with `ShaderBindings.h`; debug names stay. (b) Only popup IDs and log categories. (c) None |
| Q9 | Are `Tests/` in scope? Tests look up types and properties by name (`TypeRegistry::Find("TestDerived")`, `FindProperty("m_moveSpeed")`) even where `StaticType()` exists | (a) Out of scope: tests checking lookup-by-name should use names. (b) Use `StaticType()` wherever the type is compiled in |
| Q10 | Order and packaging | (a) Merge the gaps into 0003 T8a and 0004 as extra rows, and delete this spec. (b) Keep this as its own spec, run after 0004. (c) Run it now, before 0004, for the gaps only |

## 5a. Owner decisions (2026-10-15)

| # | Decision |
|---|---|
| Q1 | (a) Text entries with a `{}` placeholder, formatted from `StaticType().GetName()` / `AssetTypeName(…)` |
| Q2 | (a) Into `LevelFileSchema` / `AssetFileSchema`, plus one `CreateEmptyCollectionDocument()` |
| Q3 | (a) Engine data in the `AssetType` table (name, extensions, default name, creatable); editor-only data (icon, color) in an editor table indexed by `AssetType` |
| Q4 | (a) `.displayName` on property metadata and enum values |
| Q5 | (a) Extend `ProjectLayout`; add `EngineLayout` and `EditorLayout` |
| Q6 | (c) Split: user-visible defaults are text entries, internal level names are constants, starter file content goes into template files |
| Q7 | (a) Fix |
| Q8 | (a) Popup IDs, log categories (enum beside 0001's `LogSource`) and vertex semantics (shared with `ShaderBindings.h`); GPU debug names stay |
| Q9 | (a) `Tests/` out of scope |
| Q10 | (b) Own spec, run after spec 0001 |

Commits: commit and push each verified item in small chunks.

## 6. Design, TODO and acceptance criteria

Each item is re-found by its literal (line numbers above are stale). Already fixed by 0004/0001: A1 menu wiring (still string labels), B6 (icons), B7 (`AssetTypeStyle` table), C6 (`CompiledExtensions`/`WatchedExtensions`), D10 (reflected import settings), I1 (`DragDrop`), H1 (`CreateLight(LightType)`), "warning"/`#include` literals (named constants).

### TODO (in order; commit and push each)

- [x] **T1 — Type names in text (Q1).** Sentences naming an engine type become `{}` entries formatted with `StaticType().GetName()` / `AssetTypeName()`. Bare type-name labels (`AssetType*`, `LevelCollection`, tags) use the name directly and their entries are removed. Color labels for types use the type name. Create-menu labels come from `AssetTypeName`.
- [x] **T2 — Schema keys (Q2).** Migration-private `$asset`, `type`, `settings`, `.meta`, `.levelobject` and `$MissingLevelObject` move into `AssetFileSchema` / `LevelFileSchema`; one `CreateEmptyCollectionDocument()`.
- [x] **T3 — Asset type table (Q3).** `AssetType` table gains default new-asset name and creatable flag; built-in assets get a `Builtin` folder constant and a `.model` extension entry; rename keeps the extension `AssetType` reports.
- [x] **T4 — Display names (Q4).** `.displayName` on `PropertyMetadata` and enum values; Transform shows Position/Rotation/Scale through it; Level View debug modes come from the reflected `RenderDebugMode`.
- [x] **T5 — Layouts (Q5).** `EngineLayout` (content, shaders, fonts), `EditorLayout` (user settings folder and files, editor content), `ProjectLayout` additions (levels folder, `Assets/` prefix, build platform folders, binary naming), tool discovery constants.
- [x] **T6 — Default names (Q6).** User-visible defaults become text entries; internal level names become constants; starter Screen/Widget/StyleSheet content moves to template files.
- [x] **T7 — Small identifier sites (Q7).** Type redirects take the new type as a template argument; `EngineObject` type name from one constant; subsystems register by type; header tool macro names and CLI flags as named constants.
- [x] **T8 — Paired identifiers (Q8).** Popup IDs owned by one constant per popup; log categories become a `LogCategory` enum; vertex semantics shared constants; GPU debug names stay.
- [x] **T9 — Verify.** `BrightTests`, editor screenshot compare, cooked output compare.

