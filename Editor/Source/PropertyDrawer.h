#pragma once

#include "Core/Guid.h"
#include "Reflection/EngineObject.h"

#include <functional>
#include <string>

namespace BrightBox3D::Level
{
    class Level;
}

namespace BrightBox3D::Editor
{
    enum class PropertyContextAction
    {
        Revert,
        Apply
    };

    struct PropertyDrawOptions
    {
        bool instanceContext = true;
        bool readOnly = false;
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
        static bool DrawValue(const char* label, const Reflection::ValueDescriptor& descriptor, void* value, bool readOnly, const Level::Level* level);
        static bool IsVisible(const Reflection::PropertyInfo& property, const PropertyDrawOptions& options);
        static bool IsEditable(const Reflection::PropertyInfo& property, const PropertyDrawOptions& options);
        static bool AssetField(const char* label, const std::string& assetType, Core::Guid& guid, bool readOnly);
        static bool ClassField(const char* label, const Reflection::TypeInfo* baseType, std::string& typeName);
        static void BeginRow(const char* label, bool bold);
        static void EndRow();
    };
}
