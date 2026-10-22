# Spec 0009: Editor redesign — concept A "Quiet"

Status: **draft — owner chose concept A, open questions in §6**
Written: 2026-10-13

## 1. Goal

Make the editor look modern, minimal and easy to read, and move it away from the Unity look. The owner chose **concept A, "Quiet"** from the design canvas.

Main complaints about today's editor (owner, 2026-10-13):

- It looks too much like Unity.
- The Play/Pause/Step row is a copy of Unity's and takes a whole row of screen space.
- The Level View should be a permanent view, as in Unreal, not a tab. The play controls belong inside it.

## 2. References

In `specs/shared/0009-references/`; the two `reference-*` images are kept privately in `specs/personal/0009-references/`:

| File | What it is |
|---|---|
| `concept-a-edit.html` | **The target design**, edit mode. Open in a browser (the `support.js` runtime isn't present locally, so `{{accent}}` holes won't fill; the live version is on the canvas) |
| `concept-a-playmode.html` | The target design during PlayMode |
| `current-editor.webp` | Today's editor, for before/after comparison |
| `reference-viewport-play-controls.webp` | The owner's reference for play controls inside the viewport |
| `reference-midnight-style.webp` | A style the owner liked but didn't choose. Kept for context only |

Live canvas with all four concepts (A edit, A PlayMode, B, C): https://claude.ai/artifact/5G6uuVZxMKHoLZmF9AdUHZ (private to the owner).

## 3. The design, part by part

### 3.1 Window layout

```
┌───────────────────────── top bar (42 px) ─────────────────────────┐
│ ◆ File Edit Game Window     ArenaDemo / Main •      ● Reloaded  Compile ⤓ ⚙ │
├────────────┬──────────────────────────────────────┬───────────────┤
│ Level      │ Level View (permanent, no tab)        │ Inspector     │
│ Contents   │  [tools]      [▶ Play ⏸ ⏭ ▾]   [view] │               │
│ (264 px)   │                                      │ (328 px)      │
│            │  axes                       stats    │               │
│            ├──────────────────────────────────────┤               │
│            │ [Assets] [Terminal]   bottom panel   │               │
│            │ (214 px, collapsible)                │               │
└────────────┴──────────────────────────────────────┴───────────────┘
```

- The separate toolbar row and the status bar are removed.
- Level Contents and the Inspector run the full height. The bottom panel sits only under the Level View.
- The Level View has no tab header. It is always there (Q1 for how this fits spec 0001's multiple Level Views).

### 3.2 Top bar

Replaces the ImGui main menu bar and the toolbar row.

- Left: an app mark (a small accent-colored rounded square with the cube icon), then the File / Edit / Game / Window menus as plain text buttons.
- Center: `Project / Level`, with a dot when the level has unsaved changes. During PlayMode the dot becomes a blue **Playing** chip.
- Right:
  - Compile status (green dot + "Reloaded · 0.1s", or the compiling / failed state from `HotReload::GetStatusText`).
  - A **Compile** button.
  - Icon buttons for Build Game and Preferences.
- Everything here is a command (spec 0003); the window title can stay as it is.

### 3.3 Level View overlays

Three floating bars at the top of the Level View, drawn over the 3D image:

| Position | Contents |
|---|---|
| Top left | Move / Rotate / Scale (the active one filled with the accent color), a divider, World/Local, Snap, Grid (accent-colored when on) |
| Top center | **Play** (accent-filled, icon + label), Pause, Step (disabled until playing), a divider, `▾` play options (spec 0008) |
| Top right | Projection (Perspective), debug view (Lit ▾), camera speed |

- Bottom left: the X/Y/Z axis indicator.
- Bottom right: `120 fps · 86 draws · 2 lights` in a mono font, replacing the status bar's stats.
- Bars: rounded (≈11 px), dark and semi-transparent with a thin light border. ImGui has no background blur, so they're solid with alpha (§5).

### 3.4 Level Contents

- Header: **Level** title, object count, `+` create button. No tab look.
- A search field with an icon, rounded, on a darker field background.
- Rows: 28 px, rounded 6 px, an icon per type in its own color (lights yellow, PlayerStart green, linked objects blue), name, and a dim type or count chip on the right (`Collection`, `Variant`, `ArenaPlayer`).
- Selected row: accent tint at about 16% with white text. No accent bar and no zebra stripes (Q4).
- Collections are top-level rows with a chevron and a layers icon.

### 3.5 Bottom panel

- Tabs as compact segmented buttons (**Assets**, **Terminal**), the active one on a raised background.
- Terminal: Info / Warnings / Errors counters as colored text buttons, a filter field, Clear and Collapse icon buttons on the right. Log lines are 20 px, with mono, dim timestamps.
- The panel collapses to its header (Q3).

### 3.6 Inspector

- Header:
  - An icon tile (38 px rounded square) in the type's color.
  - The object name, editable in place, 15 px semibold.
  - Below it: the class name, plus the linked asset as a link (`LevelObject · Crate.variant`).
  - The Active checkbox on the right.
- A **Search properties** field.
- Sections (components): divided by thin lines, not boxes. Each header has a chevron, an enabled checkbox and a `⋯` menu on the right.
- Property rows: label column 96 px, then the value.
  - Vectors are three fields with colored axis letters inside them (X red, Y green, Z blue).
  - Asset fields show a small thumbnail or swatch before the name.
  - Overridden properties: the label in the Linked/Override color with a dot.
- **Add Component**: a full-width button with a dashed border.

### 3.7 PlayMode look

From `concept-a-playmode.html`:

- The Level View gets a 2 px inset outline in the PlayMode color.
- The play bar shows **Stop** (PlayMode color, filled), Pause, Step, and the elapsed time `00:12.4`.
- A small chip under the play bar: "Editing live objects — changes revert on stop", with a **Keep selected** button. It replaces today's banner.
- Level Contents shows a **Runtime** collection with the spawned objects.
- The Inspector shows a blue "Keep this object's changes when PlayMode stops" row with a toggle.
- PlayMode in the Level View itself (spec 0008 target 1) hides every editing overlay.

### 3.8 Visual language

| Token | Value in the concept | Notes |
|---|---|---|
| Window background | `#111113` | |
| Panel background | `#141416` | |
| Raised (active tab, buttons) | `#1F1F23` | |
| Field background | `#0D0D0F` | |
| Lines | `#1F1F23` / `#26262B` | |
| Text / dim / faint | `#E6E6E9` / `#A1A1AA` / `#8B8B94` | Dim text passes 4.5:1 on panels |
| Accent | `#FF8A3D` (today's default) | Still a user preference |
| PlayMode | `#3D8BFF` | Today's PlayMode color |
| Axes | X `#F26D6D`, Y `#7BD88F`, Z `#6EA8FF` | Y is new (`EditorColor` has no AxisY yet) |
| Radii | Rows 6, fields 6–7, buttons 7–8, floating bars 11–12, icon tile 10 | |
| Type | Geist 12.5–13 px UI, 15 px headings; Geist Mono for numbers and logs | Q2 |

Every new color is an `EditorColor` entry (CLAUDE.md rule), editable in Preferences.

## 4. Fit with other specs

| Spec | Interaction |
|---|---|
| 0001 §3.7 | Multiple Level Views, tab `+`, closing the last Level View. Conflicts with a permanent, untabbed Level View (Q1) |
| 0001 §3.6 | The status bar error count moves (Q5) |
| 0001 §3.3 | The Assets panel layouts live in the bottom panel |
| 0003 | All labels are `EditorText`, all buttons are commands. The play bar and tool bars are `DrawToolbar` lists |
| 0004 S10 | The default dock layout table describes this layout |
| 0004 S11 | Sizes in §3 become `EditorMetrics` constants |
| 0008 | The `▾` button and PlayMode targets |

## 5. ImGui feasibility notes

- No backdrop blur: the floating bars use solid colors with alpha.
- Dashed borders (Add Component) need a custom draw-list line, or a solid border instead.
- Axis letters inside vector fields: draw the letter in a fixed-width prefix, then `InputFloat` with no frame on the right.
- Floating viewport bars: child windows or draw-list groups positioned over the Level View image, with input routed so clicks on them don't pick objects.
- The top bar replaces `BeginMainMenuBar`. Menus open from the plain buttons with `BeginPopup`.

## 6. Open questions (owner decisions needed)

| # | Question | Options |
|---|---|---|
| Q1 | Permanent Level View vs spec 0001's multiple Level Views | (a) One main Level View, always present, not a tab and not closable. Extra Level Views from 0001 open as normal tabs or floating windows. (b) Only one Level View ever; drop that part of 0001. (c) The main one is permanent, extra ones split the center area (Unreal's 2×2 layouts) |
| Q2 | Font | (a) Geist + Geist Mono (SIL OFL, fetched by CMake like Lucide). (b) Keep the system fonts used today. (c) Another font you pick |
| Q3 | Bottom panel | (a) Collapsible to its header, size remembered. (b) Fixed height, resizable by dragging. (c) Also allow docking Assets/Terminal elsewhere |
| Q4 | Row style in trees and lists | (a) As in the concept: no zebra stripes, no accent bar, a tinted selection. Update the CLAUDE.md `PushSelectionColors` rule to match. (b) Keep stripes and the accent bar from today |
| Q5 | Where do warning and error counts go without a status bar? | (a) Colored counts on the Terminal tab. (b) Next to the compile status in the top bar. (c) Both |
| Q6 | Docking freedom | (a) Fixed layout as drawn; panels can't be undocked. (b) Panels stay dockable, and this is only the default layout and look |
| Q7 | Implementation order | (a) After 0003/0004, before 0001 (0001's panel work then builds on the new look). (b) After 0001. (c) Visual tokens and the top bar first; the layout changes after 0001 |

## 7. Design, TODO and acceptance criteria

To be written after the §6 decisions. Acceptance includes side-by-side screenshots against `concept-a-edit.html` and `concept-a-playmode.html` (`--screenshot`, `--play-screenshot`), and a check that `EditorLayout.ini` from before the change either migrates or resets cleanly.
