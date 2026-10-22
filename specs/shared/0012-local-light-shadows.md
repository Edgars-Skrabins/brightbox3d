# Spec 0012: Shadows for point and spot lights

Status: **done** (approved 2026-10-22, implemented 2026-10-22)
Written: 2026-10-22
Last updated: 2026-10-22

## 1. Goal

Point and spot lights cast shadows, like the directional light does today. `specs/shared/SPEC.md` §10 (line 282) and §15 (line 335) list this as out of scope for v1; this spec brings it in.

Owner direction (2026-10-22):

| Topic | Decision |
|---|---|
| Feature | Shadow casting for point and spot lights |
| Details | Decided in §5a |

## 2. Current state

### Light component

`Engine/Source/Rendering/Light.h`: one `Light` component with `LightType { Directional, Point, Spot }`, color, intensity, range, inner/outer spot angles and `m_castShadows` (category `Shadows`, **default `true` for every type**). `Light` does not override `IsPropertyVisible`, so spot angles show for every type.

### Light collection

`Engine/Source/Rendering/DrawCollector.cpp:195` `CollectLights` packs up to `MaxLights` (128) into `LightConstants`. Only the **first directional light** with `GetCastShadows()` gets a shadow (line 221); the flag is ignored for point and spot lights.

`GpuLight` (`RenderConstants.h:50`):

| Field | Contents |
|---|---|
| `positionRange` | position, range |
| `directionType` | forward direction, type |
| `colorIntensity` | color, intensity |
| `spotShadow` | x = cos inner, y = cos outer, z = shadow flag, **w = unused** |

### Shadow pass

`Engine/Source/Rendering/ShadowPass.cpp`: cascaded shadow maps for the one directional light.

- One `Texture2DArray`, `Depth32Float`, 4 layers (`CascadeCount`), resolution from `RenderView::shadows.resolution` (default 2048, clamped 256–8192).
- Each cascade: fitted orthographic matrix, own render pass on its layer, casters culled against the cascade frustum, `ShadowDepth` pipeline with slope bias.
- Writes `ShadowConstants` (4 matrices, splits, texel sizes, params) into `FrameLighting`.

`FrameLighting::Bind` binds `ShadowConstants` to `b4`, the shadow map to `t12` (or `GpuResourceCache::GetDummyShadowMap()`), and a comparison sampler (linear, white border, `LessEqual`) to `s12`.

`DrawCollector.cpp:147` already keeps off-screen meshes that cast shadows, so off-screen casters are available to any shadow view.

### Shaders

`Engine/Content/Shaders/Lighting.hlsl`: `EvaluateLight` applies `DirectionalShadow` (3×3 PCF, cascade blend, normal offset) only in the directional branch. Point and spot lights get distance and cone attenuation only.

`EvaluateAllLights` is shared by `DeferredLighting.hlsl` (opaque) and `Forward.hlsl` (transparent), so a change in `EvaluateLight` reaches both.

`RenderDebugMode::Shadows` (`DeferredLighting.hlsl:47`) shows the directional shadow term only.

### Settings

`RenderView::ShadowSettings` (`RenderView.h:56`): enabled, resolution, distance, split lambda, bias, normal bias. `Framework::Camera` exposes only `m_shadowDistance`. `Editor/Source/Thumbnails.cpp:132` turns shadows off for thumbnails.

### RHI

| Capability | State |
|---|---|
| `TextureType` | `Texture2D`, `TextureCube`, `Texture2DArray`. No cube array |
| Render into one layer/face | Yes: `DepthAttachment::layer` (Metal `slice`, DX11 per-layer depth view) |
| Viewport / scissor per pass | Yes (`Viewport` in `RHI/Types.h`), needed for atlas tiles |
| Metal cube depth texture | `MetalResources.mm:24` maps `TextureCube`; not yet used for depth |
| DX11 | Cube and array views exist in `DX11Device.cpp:113–155`; never compiled |

### Tests and sample

- `Tests/Rendering/RendererTests.cpp:34` `RendersLitSceneWithShadows`: a sun and a point lamp, asserts `shadowCasters > 0`, writes `BrightBox3D_RendererTest.png`.
- `Samples/ArenaDemo` (authored by `Tests/System/ArenaDemoTests.cpp:128`) has one directional and one point light. The point light has `m_castShadows` at its default `true`, so the sample will look different as soon as point shadows work (Q4).

## 3. What the change requires

| # | Part | Work |
|---|---|---|
| R1 | Shadow map storage | Depth storage for several local lights at once (Q1) |
| R2 | Shadow views | Spot: one perspective view (FOV = 2 × outer angle, far = range). Point: six 90° views, one per cube face. A point light costs 6 shadow renders, a spot light 1 |
| R3 | Light selection | Choose which point/spot lights get a shadow this frame within a budget (Q2, Q3); skip lights whose range does not touch the camera frustum |
| R4 | Caster culling | Per view: frustum test (as `RenderCascade` does) plus the light's range sphere |
| R5 | GPU data | Per shadowed light: matrix (spot) or position, near and far (point), atlas/layer location, bias. `GpuLight.spotShadow.w` becomes the shadow index (−1 = none). New `LocalShadowConstants` cbuffer and binding slots in `ShaderBindings.h` |
| R6 | Shaders | `EvaluateLight` samples the local shadow for point/spot; a spot projection lookup, a cube-face lookup for point lights, PCF, and bias that grows with distance (perspective texels are not uniform like cascades) |
| R7 | Pass structure | A new `LocalShadowPass` (one responsibility per file) next to `ShadowPass`, registered in `Renderer.cpp:89`, filling `FrameLighting` |
| R8 | Settings | Per-light and/or global options (Q5, Q6), reflected so the Inspector shows them |
| R9 | Editor | Shadow debug view (Q9). Optional: `Light::IsPropertyVisible` so spot and shadow options show only for the types that use them |
| R10 | RHI | Only if Q1 picks cube arrays: a `TextureCubeArray` type in Metal and DX11 |
| R11 | Tests | Renderer test with a shadowed spot and point light; screenshot check; budget/selection unit tests on the collector |
| R12 | Docs and sample | `specs/shared/SPEC.md` §10, §15, §17; regenerate the ArenaDemo sample if Q4 changes it |

### Size

Medium. It follows the existing structure (passes, `FrameLighting`, `GpuLight`, `Lighting.hlsl`) and needs no architecture change. Expected scope: one new pass class, `DrawCollector`, `FrameLighting`, `RenderConstants.h`, `ShaderBindings.h`, `GpuResourceCache` (dummy local shadow map), `Lighting.hlsl`, `DeferredLighting.hlsl`, `Light.h`, possibly `Camera` or project settings, tests and docs. RHI work only for Q1 (b).

Main risks:

- **Cost:** each point light renders the scene six times. The budget (Q2) and caching (Q7) decide whether scenes with several lamps stay fast.
- **Bias tuning:** acne and light leaking at contact points are harder with perspective shadow maps than with cascades.
- **DX11:** the Windows path cannot be checked on this machine.

## 4. Proposed shape

Outline written before the §5 decisions; §6 is the full design.

```cpp
BrightClass(.category = Categories::Rendering, .icon = Icon::Lightbulb)
class Light : public Level::LevelObjectComponent
{
    ...
    BrightProperty(.edit = Edit::Anywhere, .category = Categories::Shadows)
    bool m_castShadows = ...;
};
```

```
Renderer passes: ShadowPass (directional, CSM) → LocalShadowPass (point, spot) → GBufferPass → ... → LightingPass
```

## 5. Open questions (owner decisions needed)

| # | Question | Options |
|---|---|---|
| Q1 | Shadow map storage | (a) One 2D depth **atlas** for all local lights: a spot light takes one tile, a point light six tiles; one texture and slot; no RHI change; cube faces looked up by hand in the shader. (b) Spot lights in a `Texture2DArray`, point lights in a **cube map array** with hardware cube sampling; needs a new RHI `TextureCubeArray` in Metal and DX11 and two slots. (c) One `Texture2DArray` for both: one layer per spot, six layers per point light; no RHI change; every light uses the same resolution |
| Q2 | Budget per frame | (a) Fixed counts, e.g. up to 4 shadowed point lights and 8 shadowed spot lights. (b) A fixed number of shadow views (e.g. 24), where a point light uses 6 and a spot light 1. (c) Atlas space: lights take tiles until the atlas is full (only with Q1 a) |
| Q3 | Which lights win when over budget | (a) Ranked each frame by importance: intensity, range and distance to the camera, lights touching the view first. (b) A per-light priority property. (c) Order in the Level |
| Q4 | `m_castShadows` default | (a) Keep `true` for every type: existing point lights (the ArenaDemo lamp) start casting. (b) Default `false` for point/spot and `true` for directional, set when the type changes. (c) Default `false` for all; the sample and templates turn it on explicitly |
| Q5 | Per-light settings | (a) Cast shadows on/off only; quality is global. (b) Add per-light shadow resolution (an enum such as Low / Medium / High), bias and normal bias. (c) (b) plus a near plane |
| Q6 | Global settings | (a) New properties on `Framework::Camera`, next to `m_shadowDistance`. (b) Project settings (a rendering quality section). (c) Named constants only for now |
| Q7 | Update frequency | (a) Re-render every shadowed light every frame. (b) Cache: re-render a light only when it or a caster inside its range moved. (c) Every frame now, caching as a later spec |
| Q8 | Filtering | (a) Same 3×3 PCF as the directional light. (b) Softer filter for local lights (5×5 or Poisson disc) |
| Q9 | Shadow debug view | (a) Shows directional and local shadows combined. (b) Directional only, as today |
| Q10 | Fade with camera distance | (a) Local shadows fade out at `shadows.distance`, like cascades. (b) A separate distance for local shadows. (c) No fade; the light's range limits them |
| Q11 | Point light depth | (a) Standard hardware depth per cube face; the shader rebuilds the compare depth from the major axis. (b) Linear distance written to an `R32Float` color target; simpler compare, extra color target and pipeline |
| Q12 | Inspector | (a) `Light::IsPropertyVisible` hides spot angles for non-spot lights and shadow options when shadows are off. (b) Leave the Inspector as it is |

## 5a. Owner decisions (2026-10-22)

| # | Decision |
|---|---|
| Q1 | One 2D `Depth32Float` atlas for all local lights. A spot light takes one tile, a point light six (one per cube face); the shader picks the face by hand. No RHI change |
| Q2 | Atlas space is the budget: lights take tiles in rank order until the atlas is full; the rest get no shadow |
| Q3 | Automatic importance each frame. Lights whose range does not touch the camera frustum are skipped; the rest are ranked by on-screen influence (intensity, range, distance to the camera). Tile size steps down with rank, never above the light's own maximum (Q5) |
| Q4 | `m_castShadows` stays `true` by default for every type. The ArenaDemo lamp casts shadows after the change and the sample is regenerated |
| Q5 | Point and spot lights get **Shadow Resolution** (enum Low / Medium / High = maximum tile 512² / 1024² / 2048², default Medium), **Bias** and **Normal Bias** |
| Q5b | These per-light options apply to point and spot lights only. The directional light keeps the global cascade settings in `RenderView::ShadowSettings` |
| Q6 | Global settings are new `Framework::Camera` properties next to `m_shadowDistance`: **Local Shadows** (on/off), **Local Shadow Atlas Size** (enum 4096 / 8192, default 8192 = 256 MB), and the Q10 distance. They flow into `RenderView::ShadowSettings` |
| Q7 | Every shadowed light is re-rendered every frame. Caching static lights is a later spec |
| Q8 | Local lights use a softer filter than the directional 3×3 PCF: a Poisson disc of about 16 comparison samples |
| Q9 | The Shadows debug view shows the directional and local shadow terms combined (darkest wins) |
| Q10 | A separate **Local Shadow Distance** on the Camera. Local shadows fade out towards it, and lights beyond it get no atlas space |
| Q11 | Point light faces use standard hardware depth: six 90° perspective renders with the existing `ShadowDepth` pipeline; the shader rebuilds the compare depth from the major axis |
| T1 display | The Inspector's global float format adapts to small values: values of 0.1 and above keep `%.3f`; smaller ones get up to 6 decimals until they show about three significant digits (Shadow Bias reads 0.0005). Vector fields use the precision of their smallest component. Drag speed is unchanged |
| T2 API | `Rendering/LocalShadowSelection.h` holds `MinShadowTileSize`, `LocalShadowCandidate` (light index, type, position, direction, range, outer angle, intensity, max tile size, bias, normal bias) and `RankedLocalShadow` (candidate index, score, tile size). Functions in `Rendering::LocalShadowSelection`: `Score(candidate, cameraPosition)`, `RequestedTileSize(candidate, view)`, `Rank(candidates, view, frustum)`; the caller passes the camera frustum |
| T3 API | `ShadowTile` (x, y, size in texels) and `ShadowAtlasAllocator` with `Reset(atlasSize)`, `AllocateTiles(requestedSize, count)` (all tiles one size, halving to `MinShadowTileSize`, empty when it does not fit or would pass `MaxLocalShadowViews`, atlas unchanged on failure) and `GetAllocatedTileCount()`. Spot lights request 1 tile, point lights 6. `MaxLocalShadowViews` lives in `RenderConstants.h` from T3 |
| T6 filter radius (2026-10-22) | The local shadow filter radius is **1.5 texels** of the light's tile, defined once as `BRIGHT_LOCAL_SHADOW_FILTER_RADIUS` in `ShaderBindings.h`. The guard FOV (C++) and the Poisson filter (HLSL, T7) both read it. Guard: `tan(fov'/2) = tan(fov/2) × size / (size − 2 × radius)` |
| T9 sample (2026-10-22) | The ArenaDemo sample is not regenerated: T1 already added the new properties by hand (keeping GUIDs) and the lamp casts shadows by default. T9 verifies it instead (system test, packaged game) |
| Q12 | `Light` overrides `IsPropertyVisible`: spot angles only for Spot, range only for Point and Spot, Shadow Resolution / Bias / Normal Bias only for Point and Spot with Cast Shadows on |

## 6. Design

Names marked *(proposed)* and the values in §6.10 were approved by the owner on 2026-10-22.

### 6.1 Settings

`Rendering/Light.h`:

```cpp
BrightEnum()
enum class ShadowResolution { Low, Medium, High };

BrightProperty(.edit = Edit::Anywhere, .category = Categories::Shadows)
ShadowResolution m_shadowResolution = ShadowResolution::Medium;

BrightProperty(.edit = Edit::Anywhere, .category = Categories::Shadows)
float m_shadowBias = ...;

BrightProperty(.edit = Edit::Anywhere, .category = Categories::Shadows)
float m_shadowNormalBias = ...;
```

- `ShadowResolution` *(proposed)* maps to the maximum tile size in one place (`ShadowResolutionSize(ShadowResolution)`): Low 512, Medium 1024, High 2048.
- Getters and setters follow the existing `Light` style.
- `Light::IsPropertyVisible` (Q12) compares `property.GetValuePointer(this)` with the members:
  - `m_innerAngle`, `m_outerAngle`: Spot only.
  - `m_range`: Point and Spot.
  - `m_shadowResolution`, `m_shadowBias`, `m_shadowNormalBias`: Point and Spot with `m_castShadows` on.
- Existing saved lights load with the new defaults; no migration.

`Framework/Camera.h`, category `Shadows`, next to `m_shadowDistance`:

```cpp
BrightEnum()
enum class LocalShadowAtlasSize { Size4096, Size8192 };

bool m_localShadows = true;
Rendering::LocalShadowAtlasSize m_localShadowAtlasSize = Rendering::LocalShadowAtlasSize::Size8192;
float m_localShadowDistance = ...;
```

- `LocalShadowAtlasSize` *(proposed)* lives in `Rendering` next to `ShadowSettings`, so the renderer does not depend on `Framework`.
- `Camera::BuildRenderView` (`Camera.cpp:49`) copies the three values into `RenderView::ShadowSettings` as `localEnabled`, `localAtlasSize`, `localDistance`.
- `shadows.enabled` keeps switching off **all** shadows (thumbnails, `BRIGHT_NO_SHADOWS`); `localEnabled` switches off local shadows only.

### 6.2 Frame flow

```
DrawCollector::CollectLights   → GpuLight per light, plus a LocalShadowCandidate per shadow-casting point/spot light
ShadowPass                     → directional cascades (unchanged)
LocalShadowPass                → rank, allocate tiles, render views, write LocalShadowConstants, set GpuLight shadow index
GBufferPass … LightingPass     → FrameLighting::Bind binds both shadow maps; Lighting.hlsl samples them
TransparentPass                → same EvaluateAllLights, so transparent surfaces receive local shadows too
```

`LocalShadowPass` is registered after `ShadowPass` in `Renderer.cpp:89`.

### 6.3 Candidates

`DrawCollector::CollectLights` adds a `LocalShadowCandidate` *(proposed)* to `FrameLighting::localShadowCandidates` for every Point or Spot light with `GetCastShadows()`, when `shadows.enabled && shadows.localEnabled`. It holds:

- the light's index in `LightConstants`;
- type, position, direction, range, outer angle, intensity;
- maximum tile size, bias, normal bias.

`GpuLight.spotShadow.w` is set to −1 for every light. Only `LocalShadowPass` writes a real index.

### 6.4 Selection and tile sizes

`Rendering/LocalShadowSelection.h/.cpp` *(proposed)*: a namespace of free functions with no GPU access, so it can be unit tested.

1. **Skip:**
   - lights whose range sphere (as `Math::Bounds`) is outside the camera frustum;
   - lights whose range sphere lies entirely beyond `localDistance`.
2. **Score** (Q3): `intensity × range² / max(distanceToCamera², range²)`. A light the camera stands inside gets its full `intensity`.
3. **Sort** by score, highest first. Ties go to the lower light index, so the order is stable from frame to frame.
4. **Requested tile size:** the light's maximum, halved per step of screen coverage. Coverage is the range sphere's projected radius divided by half the view height, clamped to 0–1. A light filling the screen asks for its maximum; a distant small one asks for less. The size is rounded down to a power of two and never goes below `MinShadowTileSize`.

### 6.5 Atlas allocation

`Rendering/ShadowAtlasAllocator.h/.cpp` *(proposed)*: a class owned by `LocalShadowPass`, reset every frame (Q7).

- Power-of-two quadtree (buddy) allocator over the atlas. Tiles are square, power-of-two and aligned to their size.
- Lights are placed in rank order. A spot light needs 1 tile and a point light 6 tiles of the same size.
- When a light does not fit, it retries at half size, down to `MinShadowTileSize`. If it still does not fit, the light gets no shadow and its `spotShadow.w` stays −1.
- A point light that fits only some of its faces gets none: all 6 tiles or nothing.
- No more than `MaxLocalShadowViews` views are placed per frame. This caps the constant buffer and the number of renders.

### 6.6 Rendering

`Rendering/LocalShadowPass.h/.cpp`, structured like `ShadowPass`:

- **Atlas texture:** `EnsureAtlas(size)` creates one `Texture2D`, `Depth32Float`, `DepthStencil | Sampled`, debug name `LocalShadowAtlas`. It is recreated only when the size changes, with `WaitIdle` first, like `EnsureShadowMap`. With no shadowed local light the atlas is not created and the dummy is bound.
- **Views:**
  - Spot: `LookAtLH(position, position + direction)` × `PerspectiveFovLH(2 × outer angle + guard, 1, ShadowNearPlane, range)`.
  - Point: six faces (+X, −X, +Y, −Y, +Z, −Z) with the standard cube up vectors, each `PerspectiveFovLH(90° + guard, 1, ShadowNearPlane, range)`.
  - The **guard** widens each FOV by the filter radius in texels for that tile size, so Poisson samples near a tile edge still read this light's depth rather than a neighbouring tile's.
- **One render pass** over the whole atlas with `LoadAction::Clear`. Then, per view: `SetViewport` and `SetScissor` to the tile, `ViewConstants` with the view matrix, cull casters, `DrawItems`.
- **Caster culling:** the item's world bounds must intersect both the view frustum and the light's range bounds; only items with `castShadows` are drawn.
- **Pipeline:** `GetMeshPipeline(ShaderProgram::ShadowDepth, CullMode::None, {}, Depth32Float, DepthWrite::On, {}, DepthClip::On, 0, ShadowSlopeBias)`. Depth clip stays on, unlike cascades, because perspective views have a real near plane.
- **Statistics:** `RenderStatistics` gains `localShadowLights` and `localShadowViews`; local casters add to `shadowCasters`. The status bar is unchanged.

### 6.7 GPU data

`RenderConstants.h`:

```cpp
constexpr uint32_t MaxLocalShadowViews = 64;

struct LocalShadowView
{
    Math::Matrix4 viewProjection;
    float atlasRect[4];
    float params[4];
};

struct LocalShadowConstants
{
    float settings[4];
    LocalShadowView views[MaxLocalShadowViews];
};
```

- `atlasRect`: tile offset and scale in atlas UV.
- `params`: bias, normal bias × world texel size at 1 m distance, near plane, range.
- `settings`: local distance, fade start, atlas texel size, enabled.
- `GpuLight.spotShadow.w` holds the first view index. A point light's faces are `index + face` in +X, −X, +Y, −Y, +Z, −Z order.

`ShaderBindings.h` gains `BRIGHT_SLOT_LOCAL_SHADOW_CONSTANTS 5` and `BRIGHT_SLOT_LOCAL_SHADOW_ATLAS 13`. The comparison sampler on `s12` is shared.

`FrameLighting` gains `localShadowConstants` (bytes, like `shadowConstants`), `localShadowAtlas`, and `localShadowCandidates`. `Bind` binds the constants and the atlas, or `GpuResourceCache::GetDummyLocalShadowAtlas()`: 1×1 `Texture2D` `Depth32Float`, created next to `m_dummyShadow`.

### 6.8 Shaders

`Lighting.hlsl`:

- `cbuffer LocalShadowConstants : BRIGHT_B(BRIGHT_SLOT_LOCAL_SHADOW_CONSTANTS)` and `Texture2D LocalShadowAtlas : BRIGHT_T(BRIGHT_SLOT_LOCAL_SHADOW_ATLAS)`.
- A constant 16-point Poisson disc (Q8).
- `SampleLocalShadowView(view, worldPosition, normal, distanceToLight)`:
  1. Offset the position along the normal by `normalBias × distanceToLight` (perspective texels grow with distance).
  2. Project, map into the tile with `atlasRect`, and subtract `bias`.
  3. Take 16 `SampleCmpLevelZero` taps, clamped to the tile rect.
- `PointShadowFace(toPixel)`: the major axis picks the face (Q11). The face's own matrix gives the compare depth, so no separate depth reconstruction is needed.
- `LocalShadow(light, worldPosition, normal, viewDistance)`:
  - returns 1 when `spotShadow.w < 0`;
  - otherwise samples the spot view or the point face, and fades to 1 over the last part of `localDistance`, as `DirectionalShadow` fades its last cascade (Q10).
- `EvaluateLight` multiplies `attenuation` by `LocalShadow(...)` in the point/spot branch, after the early range test. `EvaluateLight` and `EvaluateAllLights` gain no new parameters beyond what they already receive.
- Shadows debug view (`DeferredLighting.hlsl:47`, Q9): the result is the minimum of `DirectionalShadow` and `LocalShadow` for every light whose range contains the pixel.

### 6.9 Editor

- **Inspector:** follows `IsPropertyVisible` (§6.1) with no editor code.
- **Camera Shadows category:** shows the three new properties.
- **No new editor text, commands or colors.** Property names are prettified from members; any that read badly get `DisplayNames` entries in `Rendering/Categories.h` and `Framework/Categories.h`.

### 6.10 Values set by this design

| Constant | Value | Where |
|---|---|---|
| `MinShadowTileSize` | 128 | `LocalShadowSelection.cpp` |
| `MaxLocalShadowViews` | 64 (about 10 point lights, or a mix) | `RenderConstants.h` |
| `ShadowNearPlane` | 0.05 m | `LocalShadowPass.cpp` |
| Maximum spot shadow FOV | 170° (wider cones are clamped for the shadow only) | `LocalShadowPass.cpp` |
| `m_shadowBias` default | 0.0005, kept after T8 tuning (no acne or face seams in the test scenes or ArenaDemo) | `Light.h` |
| `m_shadowNormalBias` default | 1.5, kept after T8 tuning | `Light.h` |
| `m_localShadowDistance` default | 40 m | `Camera.h` |
| `BRIGHT_LOCAL_SHADOW_FILTER_RADIUS` | 1.5 texels (approved 2026-10-22) | `ShaderBindings.h` |
| Fade band | last 10 % of `localDistance`, like cascades | `Lighting.hlsl` |

### 6.11 Docs and sample

- `specs/shared/SPEC.md`:
  - §10 line 282: lights list point/spot shadows.
  - §15 line 335: remove "point/spot light shadows".
  - §17: atlas, selection and allocation notes.
- `Tests/System/ArenaDemoTests.cpp`: the lamp keeps the default `castShadows`. Regenerate the sample with `BRIGHT_EXPORT_SAMPLE=1 ./build/bin/BrightTests --gtest_filter='SystemTests.*'`.
- Windows/DX11: the new code goes only through the RHI and HLSL, with no backend-specific code, but it stays unverified like the rest of DX11.

## 7. TODO

- [x] T1 Settings: `ShadowResolution`, the new `Light` properties and `Light::IsPropertyVisible`; `LocalShadowAtlasSize`, the Camera properties and `ShadowSettings` fields.
- [x] T2 `LocalShadowSelection`: skip, score, sort and requested tile size, with unit tests.
- [x] T3 `ShadowAtlasAllocator`: buddy allocation, halving fallback, all-or-nothing point lights and the view cap, with unit tests.
- [x] T4 Candidates in `DrawCollector::CollectLights`; `GpuLight.spotShadow.w` = −1 by default.
- [x] T5 GPU data: `LocalShadowConstants`, binding slots, `FrameLighting` fields and `Bind`, and the dummy atlas in `GpuResourceCache`.
- [x] T6 `LocalShadowPass`: atlas texture, spot and point views with guard FOV, per-tile viewport and scissor, caster culling, statistics; registered after `ShadowPass`.
- [x] T7 `Lighting.hlsl`: Poisson sampling, point face selection, bias, fade; applied in `EvaluateLight`; debug view in `DeferredLighting.hlsl`.
- [x] T8 Renderer tests, bias tuning against the test scene and ArenaDemo, screenshots.
- [x] T9 ~~Regenerate the ArenaDemo sample~~ (already current from T1, hand-edited to keep GUIDs; verified instead); update `specs/shared/SPEC.md`.

## 8. Acceptance

- A spot light and a point light each cast visible shadows from a box onto the floor, in the editor Level View, in PlayMode and in the packaged ArenaDemo. Opaque and transparent surfaces both receive them.
- **Selection:** a renderer test with a spot light and a point light reports `localShadowLights == 2` and `localShadowViews == 7`. With `localEnabled` off, or `castShadows` off on both lights, both are 0 and the image matches the unshadowed render.
- **Pixel check:** a pixel behind a caster is darker with local shadows on than off; a pixel in the open is unchanged.
- **Unit tests:**
  - lights outside the frustum or beyond the local distance get no shadow;
  - ranking follows the score;
  - tiles never overlap and stay inside the atlas;
  - a full atlas halves tile sizes before dropping lights;
  - a point light gets 6 tiles or none;
  - no frame exceeds `MaxLocalShadowViews`.
- **Inspector:** spot angles show only for Spot lights. Range shows only for Point and Spot. Shadow Resolution, Bias and Normal Bias show only for Point and Spot with Cast Shadows on. Checked by a test and a screenshot.
- **Camera:** the Camera Inspector shows Local Shadows, Local Shadow Atlas Size and Local Shadow Distance, and changing them affects the Level View.
- **Debug view:** the Shadows debug view shows local shadows next to the directional ones.
- **Quality:** no visible shadow acne or seams between point light faces in the test scene and in ArenaDemo at the default bias values.
- `BrightTests` passes, and the regenerated ArenaDemo runs packaged with the lamp casting shadows.
