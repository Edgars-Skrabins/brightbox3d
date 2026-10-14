#pragma once

namespace BrightBox3D::Level::LevelFileSchema
{
    inline constexpr int CurrentVersion = 1;

    inline constexpr char AssetKind[] = "$asset";
    inline constexpr char Version[] = "version";
    inline constexpr char Id[] = "id";
    inline constexpr char Name[] = "name";
    inline constexpr char Active[] = "active";
    inline constexpr char Enabled[] = "enabled";
    inline constexpr char Properties[] = "properties";
    inline constexpr char Transform[] = "transform";
    inline constexpr char Components[] = "components";
    inline constexpr char Children[] = "children";
    inline constexpr char Root[] = "root";
    inline constexpr char LevelObjectAsset[] = "levelObject";
    inline constexpr char Overrides[] = "overrides";
    inline constexpr char AttachTo[] = "attachTo";
    inline constexpr char Target[] = "target";
    inline constexpr char Component[] = "component";
    inline constexpr char Property[] = "property";
    inline constexpr char Value[] = "value";
    inline constexpr char Objects[] = "objects";
    inline constexpr char Collection[] = "collection";
    inline constexpr char Collections[] = "collections";
    inline constexpr char Settings[] = "settings";

    inline constexpr char TransformComponentKey[] = "transform";
    inline constexpr char PseudoName[] = "$name";
    inline constexpr char PseudoActive[] = "$active";
    inline constexpr char PseudoEnabled[] = "$enabled";
}
