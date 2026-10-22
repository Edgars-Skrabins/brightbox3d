# Spec 0011: Engine source in a project

Status: **stub — questions to be written**
Written: 2026-10-18

## 1. Goal

Owner direction (2026-10-18, from spec 0010): a project can include the engine source so the user can modify the engine for that game.

## 2. Topics for the questions

- Copy the engine source into the project, or point the project at an engine checkout.
- How the project builds its own engine library and player, and how long that takes.
- How the editor switches to the project's engine (restart, rebuild of the editor itself).
- Packaging: packaged games ship the project's engine build.
- Updating to a newer engine when the source was modified.
