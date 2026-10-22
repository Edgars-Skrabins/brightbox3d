# Spec 0007: One Collider component with a shape selector

Status: **implemented** (2026-10-18)
Written: 2026-10-13

## 1. Goal

Replace the four collider classes with **one `Collider` component**. The shape (box, sphere, mesh, …) is a property chosen in the Inspector, not a separate component type.

Owner direction (2026-10-13):

| Topic | Decision |
|---|---|
| Component | One `Collider` component |
| Shape | Selected inside the component: box, sphere, mesh, "etc." (exact list: Q1) |

## 2. Current state

`Engine/Source/Physics/Colliders.h`:

| Class | Properties |
|---|---|
| `Collider` (abstract, `Icon::SquareDashed`) | `m_isTrigger`, `m_center`, `m_friction`, `m_restitution` |
| `BoxCollider` | `m_size` (default 1,1,1) |
| `SphereCollider` | `m_radius` (0.5) |
| `CapsuleCollider` | `m_radius` (0.5), `m_height` (2.0) |
| `MeshCollider` | `m_convex`, `m_model` (`AssetReference<ModelAsset>`), `m_meshIndex` |

Code that branches on the collider class:

| Where | What |
|---|---|
| `Physics/PhysicsWorld.cpp:302` `CreateColliderShape` | `Reflection::Cast<BoxCollider>` / `<SphereCollider>` / `<CapsuleCollider>` / `<MeshCollider>` chain → Jolt `BoxShape`, `SphereShape`, `CapsuleShape`, `ConvexHullShape` / `MeshShape`. Several colliders on one object are combined with `CombineShapes` (`StaticCompoundShape`) |
| `Physics/PhysicsWorld.cpp:224,265,273,474` | `GetComponent<Collider>()`, `IsA<Collider>()`, `GetComponents<Collider>()` (base class only, so unaffected) |
| `Editor/Source/Panels/LevelViewPanel.cpp:192–215` | Collider gizmo drawing, the same cast chain (mesh colliders get no gizmo) |
| `Tests/Physics/PhysicsTests.cpp:20,29,70,76,93` | `AddComponent<BoxCollider>()`, `<SphereCollider>()` |
| `Tests/System/ArenaDemoTests.cpp:68,101–103,110,117,121` | Sample authoring: `CapsuleCollider`, `BoxCollider` (with `SetSize`, `SetCenter`), `SphereCollider` |
| `Samples/ArenaDemo/Assets/Levels/Main_Main.collection` | 17 × `"$type": "BoxCollider"`, 1 × `"SphereCollider"` |
| `Samples/ArenaDemo/Assets/Prefabs/Player.variant` | 1 × `"CapsuleCollider"` |

`specs/shared/SPEC.md:299` and §17 describe the four classes.

## 3. Proposed shape of the change

To be confirmed through §5. Names marked *(proposed)* need owner approval.

```cpp
BrightEnum()
enum class ColliderShape { Box, Sphere, Capsule, Mesh };

BrightClass(.category = Categories::Physics, .icon = Icon::SquareDashed)
class Collider : public Level::LevelObjectComponent
{
    BrightBody()
public:
    ColliderShape GetShape() const;
    void SetShape(ColliderShape shape);
    ...
};
```

- `ColliderShape` *(proposed)*: a reflected enum, so the Inspector shows a dropdown with no extra code.
- Common properties stay on `Collider`: trigger, center, friction, restitution.
- Shape properties: Q2 decides how they're stored and how the Inspector shows only the relevant ones.
- `CreateColliderShape` becomes one `switch (collider.GetShape())`.
- The gizmo becomes a `switch` too. Mesh shape gizmo: Q6.
- The Add Component menu shows one **Collider** entry.
- Game code: `AddComponent<Physics::Collider>()->SetShape(ColliderShape::Sphere)` *(proposed)*, plus typed setters per shape (Q2).
- Several `Collider` components on one object still combine into a compound shape.
- `Rigidbody` is unchanged.

## 4. Saved files

ArenaDemo and any user project contain the old type names. `TypeRedirectRegistration` only renames a type; it can't map `BoxCollider.m_size` into "`Collider` with shape Box and size". Q4 decides how old files load.

After the change, `BRIGHT_EXPORT_SAMPLE=1 ./build/bin/BrightTests --gtest_filter='SystemTests.*'` regenerates the sample assets from the updated system test.

## 5. Open questions (owner decisions needed)

| # | Question | Options |
|---|---|---|
| Q1 | Which shapes? ("box, sphere, mesh etc.") | (a) Box, Sphere, Capsule, Mesh — today's set. (b) Add Cylinder (Jolt `CylinderShape`). (c) Also split Mesh into **Convex Mesh** and **Triangle Mesh** instead of a `convex` checkbox. (d) Other (plane, heightfield…) |
| Q2 | How are shape-specific values stored and shown? | (a) All values live on `Collider` (`size`, `radius`, `height`, `model`, `meshIndex`, `convex`); a new property metadata field (e.g. `.visibleWhen`) hides the ones the current shape doesn't use. Reusable for other components. (b) One reflected struct per shape (`BoxShapeSettings`, …) held in the component, and the Inspector draws only the active one. (c) Shared values where they make sense (`size` for box, `radius` for sphere and capsule, `height` for capsule), and the Inspector hard-codes which to show per shape |
| Q3 | Switching shape: what happens to values? | (a) Each shape keeps its own values, so switching back restores them. (b) Values convert where possible (box size → sphere radius = half the largest side). (c) Reset to defaults |
| Q4 | Loading old `BoxCollider` / `SphereCollider` / `CapsuleCollider` / `MeshCollider` data | (a) A general type migration hook in serialization (old type name → new type + a function that rewrites the JSON properties), registered next to the class like `TypeRedirectRegistration`. (b) A one-time project migration that rewrites asset files on open, like `LevelObjectAssetMigration`. (c) No compatibility: regenerate the sample and accept that older projects lose their colliders |
| Q5 | Default when the component is added | (a) Box fitted to the object's mesh bounds (MeshRenderer), else 1×1×1. (b) Always Box 1×1×1. (c) Shape picked from the mesh: Sphere for the sphere primitive, Capsule for the capsule, Box otherwise |
| Q6 | Mesh shape | (a) Defaults to the object's own MeshRenderer mesh when no model is set. (b) Always needs an explicit model. Gizmo: draw the mesh wireframe, or the bounds only? |
| Q7 | Physics material (friction, restitution) | (a) Stays on the collider. (b) Moves into a separate physics material asset later (out of scope here) |
| Q8 | Old class names in game code | (a) Removed. Games that used `BoxCollider` fail to compile with a clear error. (b) Kept as thin deprecated aliases for one release |

## 5a. Owner decisions (2026-10-18)

| # | Decision |
|---|---|
| Q1 | Shapes: Box (cube), Sphere, Mesh. Capsule is dropped |
| Q3 | Switching shape keeps the XYZ size values; switching to Mesh selects the mesh and starts at 1:1:1 |
| Q4 | No migration: the engine is not deployed. Old collider data is dropped and the sample is regenerated |
| Q5 | Default shape is Box |
| Q2 | One shared `Size` (XYZ) for Box and Mesh; Sphere shows a single Diameter. Box→Sphere copies X into Diameter, Sphere→Box fills X=Y=Z, →Mesh starts at 1:1:1 |
| Q2b | The Inspector hides unused fields through a new `virtual bool EngineObject::IsPropertyVisible(const PropertyInfo&) const` (default true); `Collider` overrides it per shape |
| Q6 | Mesh uses the object's own MeshRenderer mesh unless a model is set; keeps the Convex checkbox; the gizmo draws the mesh wireframe |
| Q7 | Friction and restitution stay on `Collider` |
| Q8 | Old class names are removed (no aliases) |
| Player | The ArenaDemo player uses a Box collider sized like the old capsule |
| Edit hook | `virtual void EngineObject::OnPropertyEdited(const PropertyInfo&)`, called by the PropertyDrawer only for user edits; Collider converts Size/Diameter there |
| Own mesh | A `Geometry::MeshSource` interface (GetModel, GetMeshIndex) implemented by `MeshRenderer`; a Mesh collider without a model asks its object for one |

## 6. Design, TODO and acceptance criteria

To be written after the §5 decisions. Expected scope: `Colliders.h/.cpp`, `PhysicsWorld.cpp` shape creation, the Level View gizmo, the Inspector (if Q2a/b), serialization migration (Q4), the physics and system tests, regenerated sample assets, and `specs/shared/SPEC.md` §8/§17.
