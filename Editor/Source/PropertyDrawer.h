#pragma once

#include "Assets/AssetType.h"
#include "Core/Guid.h"
#include "Reflection/EngineObject.h"
#include "ValueDrawers.h"

#include <functional>
#include <string>

namespace BrightBox3D::Level
{
    class Level;
}

namespace BrightBox3D::Editor
{
    enum class RowLabel
    {
        Normal,
        Bold
    };

    enum class PropertyContextAction
    {
        Revert,
        Apply
    };

    struct PropertyDrawOptions
    {
        bool instanceContext = true;
        FieldAccess access = FieldAccess::Editable;
        const Level::Level* level = nullptr;
        std::function<bool(const Reflection::PropertyInfo&)> isOverridden;
        std::function<void(const Reflection::PropertyInfo&, PropertyContextAction)> onContextAction;
        std::function<bool(const Reflection::PropertyInfo&)> filter;
    };

    class PropertyDrawer
    {
    public:
        static bool DrawObject(Reflection::EngineObject& object, const PropertyDrawOptions& options);
        static bool DrawProperty(const Reflection::PropertyInfo& property, Reflection::EngineObject& object, const PropertyDrawOptions& options);
        static bool DrawValue(const char* label, const Reflection::ValueDescriptor& descriptor, void* value, FieldAccess access, const Level::Level* level);
        static bool IsVisible(const Reflection::PropertyInfo& property, const PropertyDrawOptions& options);
        static bool IsEditable(const Reflection::PropertyInfo& property, const PropertyDrawOptions& options);
        static bool AssetField(const char* label, Assets::AssetType type, Core::Guid& guid, FieldAccess access);
        static bool ClassField(const char* label, const Reflection::TypeInfo* baseType, std::string& typeName);
        static void BeginRow(const char* label, RowLabel style);
        static void EndRow();

    private:
        static bool DrawArrayProperty(const Reflection::PropertyInfo& property, Reflection::EngineObject& object, const PropertyDrawOptions& options, FieldAccess access, bool overridden);
        static bool DrawArrayElements(const Reflection::PropertyInfo& property, Reflection::EngineObject& object, const PropertyDrawOptions& options, FieldAccess access);
        static void DrawOverrideMenu(const Reflection::PropertyInfo& property, const PropertyDrawOptions& options, bool overridden);
    };
}
