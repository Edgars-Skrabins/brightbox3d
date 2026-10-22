# Spec 0010: Scripts folder and script creation

Status: **implemented** (2026-10-19)
Written: 2026-10-18

## 1. Goal

Give game code its own place in the editor, separate from assets, the way Unreal separates Content from C++ Classes, and let users create scripts from templates without writing the boilerplate by hand.

Owner direction (2026-10-18):

| Topic | Decision |
|---|---|
| Scripts folder | A dedicated scripts folder, separate from assets |
| Where scripts live | Scripts can only be created inside that folder |
| General + button | Offers "Script"; the new script is placed in the scripts folder automatically |
| Templates | Creating a script asks for an initial template: component, playable character, etc. |
| Add Component | The Add Component popup also has a Create Script button |

## 2. Current state

- Game code lives in `<Project>/Source/` (`Runtime::ProjectLayout::SourceFolder`). `cmake/BrightGame.cmake` globs `Source/**/*.cpp|*.h`; `HotReload` watches the folder and recompiles on save.
- The Assets panel shows only `<Project>/Assets/` plus the read-only `Builtin` root. Source files are invisible in the editor.
- Nothing in the editor creates code. A component written by hand (a `BrightClass` deriving `LevelObjectComponent`) compiles and appears in Add Component (verified 2026-10-18).
- `Editor/Content/AssetTemplates/` holds starter files for Screen, Widget and StyleSheet assets.
- Reflected bases a script could derive from: `Level::LevelObjectComponent`, `Level::LevelObject`, `Framework::PlayableLevelObject`, `Framework::GameModeBase`, `Framework::GameStateBase`, `Framework::PlayerController`, `Level::LevelBehavior`, `Level::LevelSubsystem`, `Framework::Camera`, UI widget controllers.

## 3. Proposed shape

To be confirmed through §5.

- The Assets panel gets a second root, **Scripts**, next to Assets (like Builtin), backed by the scripts folder on disk.
- A **Create Script** dialog: class name, template, target folder inside Scripts. It writes a `.h`/`.cpp` pair from `Editor/Content/ScriptTemplates/`, then the normal hot reload compiles it.
- Entry points: the general + button, the Scripts folder context menu, and a Create Script button at the bottom of the Add Component popup (component template preselected).
- Script creation is a command (`CommandId`), its text is `EditorText`, its paths come from `ProjectLayout`/`EditorLayout`.

## 4. Out of scope

- Editing code inside the editor (files open in the external code editor as today).
- Blueprint-style visual scripting.

## 5a. Owner decisions (2026-10-18)

| # | Decision |
|---|---|
| Q1 | The project folder `Source/` is renamed `Scripts/`. Built-in also gets a source area for the engine's own code (details: Q5) |
| Q2 | Create Script appears in the Assets panel + Create menu, the Level Contents + menu and the main menu bar |
| Q3 | Templates: Component; LevelObject and PlayableLevelObject (character); GameModeBase, GameStateBase, PlayerController, LevelBehavior |
| Q4 | After creation: hot reload compiles it, the `.cpp` opens in the code editor, and from Add Component the new component is added once compilation succeeds |
| Q5 | Builtin gets a read-only listing of the engine's own classes; opening one opens its header in the code editor. There must also be an option to include the engine source in a project so it can be modified |
| Q6 | Scripts show one item per class (the `.h`/`.cpp` pair), with the template's icon; opening it opens the `.cpp`, the context menu offers Open Header |
| Q7 | Scripts root operations: subfolders; Rename (renames both files and the class, and adds a type redirect so saved objects keep working); Delete (both files, with a confirmation). No drag-to-use |
| Q8 | Including the engine source in a project for modification is spec 0011 |

## 6. Design

### 6.1 Folder

- `ProjectLayout::SourceFolder` becomes `ScriptsFolder = "Scripts"`; `Project::GetSourceDirectory` becomes `GetScriptsDirectory`. `BrightGame.cmake` and the generated project `CMakeLists.txt` glob `Scripts/`. `HotReload` watches it.
- ArenaDemo's `Source/` moves to `Scripts/`. No migration for other projects.

### 6.2 Scripts in the Assets panel

- A `ScriptLibrary` editor service scans `Scripts/` and pairs `Name.h` + `Name.cpp` into one script entry (folder, class name, files). It rescans on file changes, like the asset database.
- The folder tree shows a **Scripts** root next to Assets and Builtin. Inside it, items are scripts and subfolders only; asset create entries are hidden there, and assets can't be dropped in.
- A script's icon is its class icon when the class is loaded (`BrightClass(.icon)`, else the base class's), or a generic code icon before the first compile.
- Open (double-click) opens the `.cpp`; the context menu has Open Header, Rename, Delete, Show in Finder, Copy Path. Rename and Delete are commands.

### 6.3 Create Script

- `ScriptTemplate` enum: Component, LevelObject, Character, GameMode, GameState, PlayerController, LevelBehavior. Each has a base class, an icon and a pair of template files in `Editor/Content/ScriptTemplates/` with a class-name placeholder.
- The Create Script dialog asks for the class name (a valid C++ identifier, not already a reflected type or an existing script), the template and the target folder (defaults to the selected Scripts folder, else the root).
- Entry points: the Assets panel + Create menu, the Level Contents + menu, the main menu (File > New Script, a command with a shortcut), and a Create Script button in the Add Component popup with Component preselected.
- After writing the files, the `.cpp` opens in the code editor and hot reload compiles. A script started from Add Component remembers the target object; when the compile succeeds and the class is loaded, the component is added (undoable). If the compile fails, nothing is added and the error shows in the Terminal.

### 6.4 Rename and delete

- Rename: the new name follows the same validation. Both files are renamed, whole-word occurrences of the old class name in the two files are replaced, and a `Reflection::TypeRedirectRegistration<NewName> s_redirect("OldName");` line is added to the `.cpp`, so saved objects keep loading.
- Delete: a confirmation dialog names the class; both files are deleted (moved to the Trash).

### 6.5 Builtin engine classes

- The header tool records each reflected class's header path relative to `Engine/Source`.
- The Builtin root gets a read-only **Engine Classes** folder: reflected engine classes grouped by module, with their icons. Opening one opens its header from the engine source when it is present; otherwise the entry is disabled with a tooltip.

## 7. TODO

- [x] T1 Rename `Source/` to `Scripts/` (layout, CMake, hot reload, project template, sample, docs).
- [x] T2 `ScriptLibrary` scan and the Scripts root in the Assets panel (both layouts).
- [x] T3 Script templates and the Create Script dialog and command; entry points in Assets, Level Contents and the main menu.
- [x] T4 Create Script in Add Component, with add-on-successful-compile.
- [x] T5 Script rename and delete.
- [x] T6 Header paths in reflection and the Builtin Engine Classes folder.
- [x] T7 Tests (scan pairing, template output compiles, rename redirect, name validation), screenshots, docs.

## 8. Acceptance

- A new project has `Assets/` and `Scripts/`; the Assets panel shows both roots plus Builtin.
- Each entry point creates a script from every template; it compiles, and a Component created from Add Component ends up on the object.
- Renaming a component script keeps a saved level loading it; deleting one turns its uses into Missing components.
- Builtin lists engine classes read-only and opens their headers.
