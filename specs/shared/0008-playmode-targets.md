# Spec 0008: PlayMode targets — in the Level View, in a window, fullscreen

Status: **draft — owner direction recorded, open questions in §6**
Written: 2026-10-13
Related: spec 0009 (design A, the play controls inside the Level View). Visual reference: `specs/shared/0009-references/concept-a-playmode.html`.

## 1. Goal

PlayMode can run in three places, picked from a menu on the Play button.

Owner direction (2026-10-13):

| Topic | Decision |
|---|---|
| Menu | **Right-clicking Play** opens the options |
| Options | 1. Play in the Level View. 2. Play in a separate window. 3. Play in a separate fullscreen window |
| Option 1 | The Level View's editing controls disappear. Only the relevant controls stay: **Stop, Pause, Play, Keep** |

## 2. Current state

- `Editor/Source/EditorPlaySession.cpp:38–61`: PlayMode always opens a new 1280×720 `Platform::Window` with its own swapchain. `Platform::Input::SetWindow` points game input at it. Escape unlocks a locked cursor. Closing the window stops PlayMode.
- `EditorPlaySession::RenderTo(commands, target)` already renders the game into any texture. `EditorApplication.cpp:762` uses it for `--play-screenshot`.
- During PlayMode the Level View shows the live play world through the editor camera (`specs/shared/SPEC.md` §17.5).
- Commands: `CommandId::Play` (toggles Play/Stop, `dynamicLabel` / `dynamicIcon`, `checkedColor = EditorColor::PlayMode`), `Pause`, `Step` in `EditorCommands.cpp:126–150`. The toolbar is `PlayToolbar[] = { Play, Pause, Step }` (`EditorApplication.cpp:55`).
- Keep changes is per object: `PlaySession::ToggleKeepChanges(id)`, shown in the Inspector and the Level Contents context menu.
- `Platform::Window` has no fullscreen API. `Platform::Input` reads one window at a time (`Input::SetWindow`).

## 3. Behavior

### 3.1 Choosing the target

- Right-clicking the Play button opens a menu with three radio entries, the current one checked:
  - Play in Level View
  - Play in New Window
  - Play Fullscreen
- The `▾` button next to Play in design A (spec 0009) opens the same menu with a left click.
- The **Game** menu lists the same three entries.
- Every entry is a command with an `EditorText` label (spec 0003 conventions). Whether choosing one also starts PlayMode is Q1. Whether the choice is remembered is Q2.

### 3.2 Target 1 — in the Level View

- The Level View renders the game camera (the possessed `PlayerController`'s camera) through `RenderTo` into the Level View's render target, with screen UI on top, at the Level View's size and aspect.
- Hidden while playing here:
  - The transform tool bar (Move/Rotate/Scale, World/Local, Snap, Grid).
  - The view options (projection, debug mode, camera speed).
  - The grid, gizmos, selection outline, light/camera/PlayerStart icons and collider wireframes.
  - The axis indicator and the stats overlay (Q7).
- Shown: the play bar only, holding **Stop** (or Play when stopped), **Pause / Resume**, **Step** (while paused) and **Keep** (Q3).
- Input:
  - Clicking into the Level View gives the game keyboard and mouse input. Game code reads mouse positions relative to the Level View, not the editor window.
  - While the game has input, editor shortcuts are off, except Stop and releasing input (Q4).
  - Cursor lock is confined to the Level View. Escape unlocks the cursor and hands input back to the editor, matching today's window behavior.
- The rest of the editor stays usable: Level Contents shows the live world, and the Inspector edits live objects.
- With several Level Views (spec 0001 §3.7), the game runs in the one where Play was pressed, or the most recently focused one (Q5).

### 3.3 Target 2 — separate window

- Today's behavior: a new window, its own swapchain, closing it stops PlayMode.
- The Level View keeps showing the live world through the editor camera, with all editing controls.
- Window size and position: Q6.

### 3.4 Target 3 — separate fullscreen window

- Like target 2, but the window covers a whole monitor.
- `Platform::Window` gains fullscreen support (GLFW `glfwSetWindowMonitor`). Borderless or exclusive, and which monitor: Q6.
- Escape unlocks the cursor. Exiting fullscreen or stopping needs a shortcut that works inside the game window: Q4.

### 3.5 All targets

- Hot reload, Pause/Step, revert on stop and Keep changes behave the same.
- The play bar shows the elapsed play time (design A).
- Automation: `--play` and `--play-screenshot` keep working. Which target they use: Q8.

## 4. Engine changes this needs

- `Platform::Input` must accept input from a region of the editor window (the Level View rect, while it has game focus), not only from a whole window. Options for the shape of this API come with the design after §6.
- `Platform::Window`: fullscreen on a chosen monitor, and back.
- `EditorPlaySession` splits "where the game is shown" from "the session", so one session can present into a window, a fullscreen window or the Level View.

## 5. Names to approve

| Name *(proposed)* | What |
|---|---|
| `PlayTarget { LevelView, Window, Fullscreen }` | The choice |
| `CommandId::PlayInLevelView`, `PlayInWindow`, `PlayFullscreen` | The three menu entries |
| `"playTarget"` in `Preferences.json` | Saved choice, if Q2 says per user |

## 6. Open questions (owner decisions needed)

| # | Question | Options |
|---|---|---|
| Q1 | Does choosing a menu entry start PlayMode? | (a) It sets the target and starts playing at once. (b) It only sets the target; the Play button then uses it (Unreal) |
| Q2 | Is the choice remembered? | (a) Per user in `Preferences.json`, for every project. (b) Per project. (c) Not remembered; default each start |
| Q3 | What does "Keep" do in target 1? | (a) Toggles Keep changes for the selected object, like the Inspector toggle. (b) Keeps every change made during this session when Stop is pressed. (c) Opens a small list of objects changed so far, each with a keep toggle |
| Q4 | Shortcuts while the game has input | Stop: (a) Cmd+P / Ctrl+P as now (the game can't use it). (b) Shift+Escape (Unreal). Releasing input / exiting fullscreen: (a) Escape when the cursor is locked, then Escape again does nothing more. (b) Shift+F1 style "eject" key |
| Q5 | Several Level Views (spec 0001) | (a) The game runs in the Level View whose Play button was used, or the last focused one. (b) Only the main Level View (if spec 0009 makes it permanent) can host the game |
| Q6 | Window and fullscreen details | Window: (a) 1280×720 centered each time. (b) Remember the last size and position per user. Fullscreen: (a) Borderless fullscreen on the monitor the editor is on. (b) Exclusive fullscreen. (c) A monitor picker in the menu |
| Q7 | Target 1 extras | Should the fps/stats overlay stay visible while playing in the Level View? Should there be an "eject" button to switch to a free editor camera without stopping (Unreal F8)? |
| Q8 | Automation flags | (a) `--play` uses the saved target. (b) `--play` always uses a window (stable for tests); add `--play-target <level-view|window|fullscreen>` |

## 7. Design, TODO and acceptance criteria

To be written after the §6 decisions. Tests should cover: target persistence, input routing (game gets input only while the Level View has game focus), editor shortcuts suppressed during game input, and a PlayMode screenshot from each target.
