# Spec 0001: Editor workflow polish

Status: **implemented** (2026-10-15)
Owner decisions recorded: 2026-10-13

**Order:** implement last, after 0003 and 0004 (owner-approved order).

**Conventions from specs 0002 and 0003, already applied below:**

- Every label, tooltip, menu entry and message in this spec is an `EditorText` entry.
- Icons are `Icon::` values; every Lucide icon is available.
- Every action with a shortcut or a menu entry is a command (`CommandId`, registered in `EditorCommands.cpp`, drawn with `CommandMenuItem` / `CommandButton`). Assets-panel shortcuts use `CommandScope::Assets`, so they win over the Global Delete/Duplicate while the Assets panel is hovered or focused.
- Panel window titles come from `EditorWindow` with fixed `###Id`s.

**Before starting, update this spec for 0004:** the Assets, Level Contents and Inspector panels are already split into smaller files, and the create menu is the `CreateMenuRegistry`. T4/T6–T9 add to those files rather than to the old monolithic panels.

## 1. Goals

1. Right-click anywhere in the Assets window opens a Unity-like context menu that includes **Create ▸**, so the Create button isn't needed.
2. Folders look like folders, and every asset type gets a real icon instead of a text badge (`DIR`, `LVL`, `MAT`, …).
3. The Assets window supports **exactly Unity's layouts**: One Column, Two Column (grid), and Two Column (list).
4. The Level Contents `+` button is centered and has a tooltip.
5. The Terminal separates **engine** messages from **game** messages, using an `Engine` filter.
6. Every docked tab group has a **`+`** after its last tab that adds another panel there, Unity-style, with multiple instances allowed.

Out of scope: favorites, labels/search-by-type filters, multi-select of assets, asset previews in the Inspector, log channels beyond Engine and Game, and floating panels as separate OS windows.

## 2. Decisions (owner-approved)

| Topic | Decision |
|---|---|
| Layouts | Exactly Unity. A `⋮` panel menu switches between **One Column Layout** and **Two Column Layout**. In Two Column, the icon-size slider at its minimum switches the item area from a grid to list rows |
| Right-click menu | Full Unity-like menu, identical on empty space, on items and in the folder tree. Entries that don't apply are greyed out, not hidden |
| Icons | All asset types get Lucide icons in their type color. Thumbnails (models, materials, textures) still replace the icon in the grid when one exists |
| Persistence | Layout and icon size are saved **per user** in `Preferences.json` and apply to every project |
| Log source | Game code logs through a new Unity-like `BrightBox3D::Debug` API, which tags entries **Game**. Everything else that goes through `Core::Log` is tagged **Engine** |
| Terminal buckets | **Info** = game `Debug::Log`. **Warnings** = compiler warnings + `Debug::LogWarning`. **Errors** = compile errors + `Debug::LogError`. **Engine** = every engine message at any severity (e.g. "Opened level", "Build failed") |
| Engine problems | They live only in the Engine bucket, still colored yellow or red. The Engine toggle turns red when the bucket holds an error, so a problem is never missed |
| Tab `+` placement | One `+` per docked tab group, right after its last tab: `[Assets] [Terminal] [+]` |
| Tab `+` content | A menu of Level View, Level Contents, Inspector, Assets and Terminal. Any panel can be opened more than once. Each Level View has its own camera; each Inspector has a lock toggle. Tabs get an `×`, and the Window menu can reopen panels |

## 3. Detailed behavior

### 3.1 Context menu (all three places)

The menu is built by one function, so every place shows the same entries in this order:

```
Create ▸            Folder
                    ──────
                    Level
                    LevelCollection
                    LevelObjectTemplate
                    Material
                    ──────
                    UI Screen
                    UI Widget
                    UI StyleSheet
──────
Show in Finder      (Windows: "Show in Explorer")
Open
Rename              F2
Delete              Del
Duplicate           Cmd+D
Copy Path           Alt+Cmd+C
──────
Set as Startup Level          (only for Levels; greyed otherwise)
Create Variant               (only for LevelObjectTemplates; greyed otherwise)
──────
Reimport
Refresh             Cmd+R
```

The context target depends on where the click lands:

| Right-click on | Target |
|---|---|
| An item | That item; it also becomes the selection |
| Empty space in the item area, or the current folder in the tree | The current folder |
| A folder in the tree | That folder |

`Create ▸` always creates inside the target folder: the clicked folder, or the folder that contains the clicked item.

These entries are greyed out when the target is the folder background, a built-in asset or the Assets root:

- Open
- Rename
- Delete
- Duplicate
- Reimport

Show in Finder on the background reveals the current folder.

Keyboard shortcuts are `CommandScope::Assets` commands, shown with platform symbols (`⌘D` on macOS). They work when the Assets window is hovered or focused and no text field is active.

Today's `+ Create` button stays in the toolbar and opens the same `Create ▸` submenu content.

### 3.2 Icons

Icons are `Icon::` values in a table indexed by `Assets::AssetType`, drawn in the type's `EditorColor::Asset*` color:

| Type | Icon |
|---|---|
| Folder | `Icon::Folder`. The tree shows `Icon::FolderOpen` when expanded |
| Level | `Icon::Map` |
| LevelCollection | `Icon::Layers` |
| LevelObjectVariant | `Icon::Package` |
| LevelObjectTemplate | `Icon::LayoutTemplate` |
| Material | `Icon::Palette` |
| Model | `Icon::Box` |
| Texture | `Icon::Image` |
| Screen | `Icon::Monitor` |
| Widget | `Icon::Component` |
| StyleSheet | `Icon::FileCode` |
| Font | `Icon::Type` |
| Other | `Icon::File` |

- **Grid:** the icon is drawn centered, sized to about 55% of the tile. To draw large glyphs, use `ImDrawList::AddText(font, size, …)` with the merged font.
- **Folders in the grid** show only the large folder icon on a transparent background: no filled square and no type stripe. Other types keep the 3px type-color stripe under the image.
- **List rows, the One Column tree and the folder tree** use the icon at text size, before the name.

### 3.3 Layouts

**Two Column Layout** (default) matches today's split:

- **Left:** the folder tree, with `Assets` and `Built-in` as roots.
- **Top of the right side**, in this order:
  - The toolbar.
  - Breadcrumbs (`Assets › Folder › Sub`). Each segment is clickable, and assets can be dropped onto it to move them there.
  - The search field.
  - The `⋮` menu button, right-aligned.
- **Item area:**
  - Grid mode: tiles, as today, with the new icons.
  - List mode: one row per asset, made of icon, name, and the type name dimmed and right-aligned. Rows use the zebra and selection style of Level Contents (`PushSelectionColors`, `RowStripe`, an accent bar on selected rows).
- **Bottom bar:**
  - Left: the selected asset's path, dimmed.
  - Right: the icon-size slider, moved there from the toolbar to match Unity.
  - Slider range: list, then 56–160 px. Its leftmost stop means list mode. Any value above it means grid mode at that tile size. Dragging between the two switches modes live.

**One Column Layout:**

- A single tree with `Assets` at the root.
- Folders and assets appear together. Folders sort first, then assets by name.
- Folders expand in place. Assets are leaves with icon and name.
- No item area, no breadcrumbs and no size slider. The bottom bar shows only the selected path.
- A non-empty search shows a flat list of matches (icon, name, dimmed path) instead of the tree.

**The `⋮` panel menu** contains:

- `One Column Layout` and `Two Column Layout`, as radio items (commands with `isChecked`).
- A separator.
- `Refresh` (the same command as Cmd+R).

**Behavior shared by both layouts:**

| Action | Behavior |
|---|---|
| Single click | Selects |
| Double click | Opens assets; enters folders in Two Column, toggles them in One Column |
| Drag and drop | Drag to the Level View or Level Contents. Drop onto folders to move. Drop OS files to import |
| Rename | Inline |
| Delete | Asks for confirmation first |

### 3.4 Persistence

Add to `EditorPreferences`, saved in the same `Preferences.json` under `"assets"`:

- `layout`: `"OneColumn"` or `"TwoColumn"`.
- `iconSize`: a number. `0` means list mode; otherwise 56–160.

Defaults: TwoColumn, 84.

Missing or invalid values fall back to the defaults. Values are saved when they change; slider changes are saved on release.

### 3.5 Level Contents `+` button

- The `+` glyph sits visibly off-center on its button because the icon font's `GlyphOffset.y = 2` is tuned for inline text. Make icon-only buttons square (`GetFrameHeight()` × `GetFrameHeight()`) and center the glyph exactly. Use one shared helper, `IconButton(Icon icon, EditorText tooltip)`, so every icon-only button is centered and has a tooltip. Icon-only buttons that run a command draw through `CommandButton`, which uses the same centering:
  - Level Contents `+`
  - The Preferences reset buttons
  - The Assets `⋮` button
- Tooltip for the Level Contents `+`: **"Create LevelObject"**.

### 3.6 Terminal: Engine vs Game messages

**Logging API (engine side)**

- Add `enum class LogSource { Engine, Game }` to `Core/Log.h` and a `LogSource source = LogSource::Engine` field to `LogEntry`.
- Change `Log::Write` to take the source as a parameter:

  ```cpp
  static void Write(LogLevel level, const std::string& message, LogSource source = LogSource::Engine, const std::string& category = {}, const std::string& file = {}, int line = 0);
  ```

  Update every existing caller that passes `category`/`file`/`line`. Today only `HotReload.cpp` does.
- Replace `Log::Count(LogLevel)` with `Log::Count(LogLevel level, LogSource source)`. Add `Log::Count(LogSource source)` for the Engine bucket total, and `Log::HasErrors(LogSource source)`.
- The status bar error count (`EditorApplication::DrawStatusBar`) shows the sum of game and engine errors.

**Game-facing API:** a new `Engine/Source/Core/Debug.h/.cpp`, exported from the engine library:

```cpp
namespace BrightBox3D
{
    class Debug
    {
    public:
        static void Log(const std::string& message, std::source_location location = std::source_location::current());
        static void LogWarning(const std::string& message, std::source_location location = std::source_location::current());
        static void LogError(const std::string& message, std::source_location location = std::source_location::current());
    };
}
```

- Each call writes with `LogSource::Game`, category `"Game"`, and the caller's file and line, so clicking the entry opens the source line like compiler errors do today.
- In the packaged player these print to stdout exactly like engine logs; no filtering happens there.

**Which source each message gets**

| Message | Source | Level |
|---|---|---|
| `Debug::Log` / `LogWarning` / `LogError` | Game | Info / Warning / Error |
| Compiler diagnostics (`HotReload` parsed warnings and errors) | Game | Warning / Error |
| "Game code compilation failed" summary | Game | Error |
| Compiler *status* lines ("Compiling game code...", "Hot reloaded ...") | Engine | Info |
| CMake missing, module load failures, asset import failures, build failures, everything else | Engine | as today |

**Terminal UI** (`TerminalPanel`)

- The toolbar, left to right:
  - `Clear`
  - `Auto-scroll`
  - `Info N`, `Warnings N`, `Errors N`: Game-source counts only
  - `Engine N`: total engine messages
  - Filter field
- Each toggle shows or hides its bucket. All four are on by default.
- The `Engine` toggle's text is `Text` when the bucket has no warnings or errors, `Warning` when it has warnings, and `Error` when it has errors. Tooltip: "Engine messages (N warnings, M errors)".
- Engine entries are shown with a small dim `Engine` tag before the message. Game entries have no tag.
- The four toggle states are saved per user in `Preferences.json` under `"terminal"`: `showInfo`, `showWarnings`, `showErrors`, `showEngine`.

**Sample:** ArenaDemo uses `Debug::Log` at least once (for example, "Arena started" in `ArenaGameMode::OnGameStart`). Edit `Samples/ArenaDemo/Source` directly; the system test copies the sample.

### 3.7 Tab `+` and multiple panel instances

**Panel model**

- Introduce `EditorPanel`, a small base class in `Editor/Source/Panels/EditorPanel.h`:
  - `virtual void Draw()`
  - `virtual EditorWindow GetWindowType() const`: `LevelView`, `LevelContents`, `Inspector`, `Assets` or `Terminal`
  - `int GetInstanceIndex() const`
  - `bool IsOpen() const` / `void Close()`
  - `std::string GetWindowName() const`. The first instance keeps `WindowTitle(type)` (`"<Title>###<Id>"`) so existing `EditorLayout.ini` files still dock correctly; later instances use `"<Title> <n>###<Id>#<n>"`.
  - Optional `virtual void SaveState(JsonValue&)` / `LoadState(const JsonValue&)`.
- The five panels derive from it. `ProjectSettingsPanel` and `PreferencesPanel` stay single, non-dockable utility windows and get no `+`.
- `EditorApplication` replaces the five `unique_ptr` members with `std::vector<std::unique_ptr<EditorPanel>> m_panels` and a factory, `CreatePanel(EditorWindow type)`.

**Per-instance state**

- **Level View:** each instance owns its camera, render target, debug mode, grid toggle and gizmo state, as the class already does. `EditorApplication::GetActiveLevelView()` (added by 0003, returning the single Level View by reference) changes to return a pointer: the most recently focused open Level View, or the first open one, or `nullptr`. Update its call sites, including the Level View commands in `EditorCommands.cpp`, to handle `nullptr` (the commands report not enabled).
- **Level Contents:** its own filter, rename state and scroll.
- **Inspector:**
  - Each instance gets a **lock** toggle (`Icon::Lock` / `Icon::LockOpen` icon button, top-right; tooltip "Lock Inspector").
  - While locked, it keeps showing the object or asset that was selected when it was locked, and ignores selection changes.
  - If that target is destroyed, it shows "Locked target no longer exists" and offers an Unlock button.
- **Assets:**
  - Each instance has its own current folder, search and rename state. Move `EditorContext::GetAssetsFolder` / `SetAssetsFolder` into the panel.
  - Layout and icon size stay global per-user preferences (§3.4).
- **Terminal:** each instance has its own search text and auto-scroll. The four toggles stay global (§3.6).

**Tab `+` button**

- After the tab bar of every dock node that has at least one tab, draw a square `IconButton(Icon::Plus, EditorText::AddTab)` vertically centered in the tab bar, just right of the last tab.
  - Implementation: after `DockSpace()`, walk the dock nodes with `imgui_internal` (`ImGui::DockBuilderGetNode`, `node->TabBar`, the last tab's rect) and draw the button in a small no-background, no-input-blocking overlay window on top of the tab bar, so it doesn't steal focus or dock targets.
  - When the tab bar is too narrow, the `+` sits at the far right of the bar instead of overlapping tabs.
- Clicking it opens a popup with one entry per panel type, each with its icon:

  | Panel | Icon |
  |---|---|
  | Level View | `Icon::Box` |
  | Level Contents | `Icon::ListTree` |
  | Inspector | `Icon::Info` |
  | Assets | `Icon::Folder` |
  | Terminal | `Icon::Terminal` |

- Choosing an entry creates a new instance, docks it into **that** node as the last tab (`DockBuilderDockWindow` before the window's first `Begin`), and focuses it.

**Closing and reopening**

- Each panel tab shows an `×`: `Begin(name, &open)`. Closing destroys that instance; Level View releases its render target.
- Closing the last Level View is allowed. The editor then has no viewport, and the play banner and picking simply don't exist.
- The **Window** menu lists one "New <Type>" command per type, which opens a floating instance, plus the existing `Reset Layout` command. `Reset Layout` restores exactly one instance of each type in the default arrangement.

**Persistence (per user)**

- Save open instances to `~/.brightbox3d/EditorPanels.json` next to `EditorLayout.ini`, as `[{ "type": "Level View", "index": 2, "state": {…} }, …]`.
- State to save:
  - Level View: camera position, rotation, speed, debug mode, grid.
  - Inspector: lock flag only; the locked target isn't restored.
  - Assets: current folder.
- Load it before the first frame so ImGui matches windows from the ini file.
- If the file is missing or invalid, create the default single instances.

## 4. Known bug to fix along the way

`AssetsPanel` fills the rest of the item area with an `InvisibleButton("##dropzone")` so it can accept drops. Because that button is an item, `BeginPopupContextWindow(..., NoOpenOverItems)` almost never opens on empty space. Right-clicking the empty area must open the context menu while drops onto the empty area keep working. Open the popup from the dropzone itself with `BeginPopupContextItem`.

## 5. TODO (follow in order; check off as done)

- [x] **T2 — IconButton helper.**
  - Add `IconButton(Icon icon, EditorText tooltip)` to `Theme.h/.cpp`: square, glyph centered by measured bounds, tooltip on hover. `CommandButton` uses the same centering for icon-only commands.
  - Use it for the Level Contents `+` (tooltip `EditorText` "Create LevelObject"), the Preferences color reset buttons and the Shortcuts reset buttons ("Reset to default").
- [x] **T3 — Preferences.** Add `AssetsLayout` (`enum class AssetsLayout { OneColumn, TwoColumn }`) and `iconSize` to `EditorPreferences`, with getters and setters, load and save under `"assets"`, and defaults. Add a unit test in `Tests/Editor/` for the save → load round trip and for invalid-value fallback.
- [x] **T4 — Context menu.**
  - Replace `DrawCreateMenu` / `DrawItemContextMenu` with a single `DrawContextMenu(const ContextTarget&)`. Keep `DrawCreateMenu(folder)` as the `Create ▸` body, with a folder parameter instead of `CurrentFolder()`.
  - Use it on items, on empty space (fix §4) and in the folder tree.
  - Add the entries as `CommandScope::Assets` commands (asset Rename, Delete, Duplicate, Copy Path, Refresh, Open, Show in Finder, Reimport, Set as Startup Level, Create Variant) with the shortcuts F2, Del, Cmd+D, Alt+Cmd+C and Cmd+R, drawn with `CommandMenuItem`. The Assets panel calls `DispatchShortcuts(CommandScope::Assets)` while hovered or focused.
  - Extend the command tests: an Assets-scope Delete wins over the Global Delete while the Assets panel dispatches.
- [x] **T5 — Duplicate.** Add `AssetDatabase::DuplicateAsset(guid)`: copy the file to a unique `"<name> 1.<ext>"` path with a new GUID `.meta`, then refresh. Folders duplicate recursively with new GUIDs. Add a unit test in `Tests/Core` or the asset tests.
- [x] **T6 — Type icons.**
  - Replace the `TypeGlyph` badge switch (and its `AssetBadge*` `EditorText` entries) with the §3.2 `Icon` table.
  - Grid folders: large icon only, no filled square and no stripe.
  - Folder tree: `folder` and `folder-open` icons.
- [x] **T7 — Two Column polish.**
  - Add breadcrumbs, which are drop targets.
  - Add the bottom bar with the selected path and the icon-size slider; remove the slider from the toolbar.
  - Make the slider's leftmost stop list mode.
- [x] **T8 — List mode.** Draw rows with icon, name and dimmed right-aligned type, zebra stripes and accent selection. Rename, drag-drop and context menu work exactly as in grid mode.
- [x] **T9 — One Column layout.** Draw the combined tree, show the flat search results, and support rename, drag-drop and the context menu.
- [x] **T10 — `⋮` menu.** Add the layout radio items and Refresh as commands, saving the layout to preferences.
- [x] **T11a — Log sources.**
  - Add `LogSource`, the `Write` signature, `Count(level, source)` / `Count(source)` / `HasErrors(source)` to `Core/Log`.
  - Update `HotReload.cpp` per the §3.6 table.
  - Update the status bar.
  - Unit tests in `Tests/Core`: source tagging, counts per source, and `Clear` resets every count.
- [x] **T11b — Debug API.**
  - Add `Core/Debug.h/.cpp` with `std::source_location`, exported from the engine library so the game module links it.
  - Unit test that file and line are captured and the source is Game.
  - Extend `HotReloadCompilesAndPreservesState` or the ArenaDemo system test so game code calls `Debug::Log`, and check that the entry arrives with `LogSource::Game`.
- [x] **T11c — Terminal UI.**
  - Game-only Info/Warnings/Errors counts, the `Engine` toggle with severity coloring and tooltip, and the dim `Engine` tag on entries.
  - Save the toggle states in `Preferences.json` under `"terminal"`; add to the T3 round-trip test.
- [x] **T11d — Sample.** Add `Debug::Log("Arena started")` to `Samples/ArenaDemo/Source/ArenaGameMode.cpp` and confirm the system test still passes.
- [x] **T11e — EditorPanel base.**
  - Create `EditorPanel` and make the five panels derive from it.
  - Add `m_panels` and `CreatePanel` to `EditorApplication`, and draw every panel in the loop.
  - First-instance window names stay `WindowTitle(type)`.
  - The build and all tests must stay green before continuing.
- [x] **T11f — Per-instance state.**
  - Make `GetActiveLevelView()` (added by spec 0003) return a pointer tracking the focused Level View instance, and fix its call sites and the Level View commands.
  - Move the Assets folder into the panel.
  - Add the Inspector lock: icon button, locked target, "no longer exists" state.
- [x] **T11g — Close and Window menu.** Add `×` on tabs, "New <Type>" commands, and make the `Reset Layout` command restore one instance of each.
- [x] **T11h — Tab `+`.**
  - Draw the overlay `IconButton(Icon::Plus, EditorText::AddTab)` after each dock node's last tab, with the popup of panel types and their `Icon::` icons.
  - Dock the new instance into that node and focus it.
  - Handle narrow tab bars.
- [x] **T11i — Panel persistence.** Save and load `EditorPanels.json` with the per-type state, falling back to defaults when it's missing or invalid. Unit test the round trip in `Tests/Editor/` without a window: serialize, parse, and compare the type list and state.
- [x] **T11 — Verify.** Run `BrightTests`. Take editor screenshots with `--frames 60 --screenshot` of:
  - Two Column grid.
  - Two Column list (`iconSize: 0` in a temporary `Preferences.json`).
  - One Column.
  - The Level Contents `+` button zoomed.
  - The Terminal with Engine on and off, after a play session that called `Debug::Log`.
  - The bottom dock area after adding a second Terminal via its `+`, then restarting the editor so it persists.
  - Two Level Views side by side.

  Add an editor automation flag, `--add-panel <EditorWindow id>`, which adds an instance to the default node of that type at startup, so the screenshot run can show a second instance.

  Check each screenshot visually. Restore the user's `Preferences.json` afterwards.
- [x] **T12 — Docs.** Update `specs/shared/SPEC.md` §17.5, the README's editor section, and `CLAUDE.md` if new conventions came up (for example the `IconButton` helper).
- [x] **T13 — Review.** Specs are private: never `git add`, commit or push anything under `docs/specs/`. Present the diff summary and screenshots to the owner. Don't commit until they approve. Then commit in small chunks:
  1. IconButton helper
  2. Preferences
  3. Context menu
  4. Duplicate
  5. Asset icons
  6. Layouts
  7. Log sources + Debug API
  8. Terminal UI
  9. Sample
  10. EditorPanel refactor
  11. Inspector lock / per-instance state
  12. Tab + and Window menu
  13. Panel persistence
  14. Docs

## 6. Acceptance criteria

- Right-clicking empty space, an item or a folder in the tree always opens the menu, and `Create ▸` creates in the right folder.
- No text badges remain. Folders render as folder icons in the grid, the list and both trees.
- All three layouts work, and the choice and size survive a restart, in any project.
- The `+` glyph is centered (checked in a zoomed screenshot), and hovering it shows "Create LevelObject".
- A `Debug::Log` from game code appears under Info only. Toggling Engine off hides "Opened level…" style messages but no game messages. Compiler errors appear under Errors. An engine error turns the Engine toggle red.
- Every tab group shows one `+` after its last tab. Adding a second Level View, Inspector, Assets or Terminal works; each Level View has its own camera, and a locked Inspector ignores selection changes. Closing and reopening work, and the set of open panels survives a restart.
- All tests pass. No hard-coded colors and no code comments are added.
