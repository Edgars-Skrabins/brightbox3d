#include "UI/Binding.h"

#include "Core/EngineText.h"
#include "Core/StringUtils.h"
#include "Level/Level.h"
#include "Serialization/ObjectSerializer.h"

#include <cstdio>

namespace BrightBox3D::UI
{
    using Reflection::PropertyFlagUIRead;
    using Reflection::PropertyFlagUIWrite;
    using Reflection::PropertyKind;

    namespace
    {
        const char* const BindingRootNames[] = { "Player", "GameMode", "GameState", "Controller", "LevelBehavior" };
        static_assert(std::size(BindingRootNames) == static_cast<size_t>(BindingRoot::Count));

        std::string BindingRootList()
        {
            std::string list;
            for (const char* name : BindingRootNames)
            {
                list += list.empty() ? name : std::string(", ") + name;
            }
            return list;
        }
    }

    const char* BindingRootName(BindingRoot root)
    {
        return BindingRootNames[static_cast<size_t>(root)];
    }

    bool FindBindingRoot(const std::string& name, BindingRoot& root)
    {
        for (size_t index = 0; index < std::size(BindingRootNames); ++index)
        {
            if (name == BindingRootNames[index])
            {
                root = static_cast<BindingRoot>(index);
                return true;
            }
        }
        return false;
    }

    std::string BoundProperty::Format(int decimals) const
    {
        const void* value = property->GetValuePointer(object);
        char buffer[64];
        switch (property->value.kind)
        {
        case PropertyKind::Bool:
            return *static_cast<const bool*>(value) ? "true" : "false";
        case PropertyKind::Int32:
            return std::to_string(*static_cast<const int32_t*>(value));
        case PropertyKind::UInt32:
            return std::to_string(*static_cast<const uint32_t*>(value));
        case PropertyKind::Int64:
            return std::to_string(*static_cast<const int64_t*>(value));
        case PropertyKind::Float:
            if (decimals >= 0)
            {
                std::snprintf(buffer, sizeof(buffer), "%.*f", decimals, *static_cast<const float*>(value));
            }
            else
            {
                std::snprintf(buffer, sizeof(buffer), "%g", *static_cast<const float*>(value));
            }
            return buffer;
        case PropertyKind::Double:
            if (decimals >= 0)
            {
                std::snprintf(buffer, sizeof(buffer), "%.*f", decimals, *static_cast<const double*>(value));
            }
            else
            {
                std::snprintf(buffer, sizeof(buffer), "%g", *static_cast<const double*>(value));
            }
            return buffer;
        case PropertyKind::String:
            return *static_cast<const std::string*>(value);
        default:
        {
            Serialization::JsonDocument document;
            Serialization::JsonValue json;
            Serialization::WriteProperty(*property, *object, json, document.GetAllocator());
            return json.IsString() ? json.GetString() : Serialization::WriteJson(json, Serialization::JsonFormat::Compact);
        }
        }
    }

    bool BoundProperty::ReadNumber(float& result) const
    {
        const void* value = property->GetValuePointer(object);
        switch (property->value.kind)
        {
        case PropertyKind::Float:
            result = *static_cast<const float*>(value);
            return true;
        case PropertyKind::Double:
            result = static_cast<float>(*static_cast<const double*>(value));
            return true;
        case PropertyKind::Int32:
            result = static_cast<float>(*static_cast<const int32_t*>(value));
            return true;
        case PropertyKind::UInt32:
            result = static_cast<float>(*static_cast<const uint32_t*>(value));
            return true;
        case PropertyKind::Int64:
            result = static_cast<float>(*static_cast<const int64_t*>(value));
            return true;
        case PropertyKind::Bool:
            result = *static_cast<const bool*>(value) ? 1.0f : 0.0f;
            return true;
        default:
            return false;
        }
    }

    bool BoundProperty::ReadBool(bool& result) const
    {
        float number = 0.0f;
        if (!ReadNumber(number))
        {
            return false;
        }
        result = number != 0.0f;
        return true;
    }

    bool BoundProperty::WriteNumber(float number) const
    {
        void* value = property->GetValuePointer(object);
        switch (property->value.kind)
        {
        case PropertyKind::Float:
            *static_cast<float*>(value) = number;
            break;
        case PropertyKind::Double:
            *static_cast<double*>(value) = number;
            break;
        case PropertyKind::Int32:
            *static_cast<int32_t*>(value) = static_cast<int32_t>(std::lround(number));
            break;
        case PropertyKind::UInt32:
            *static_cast<uint32_t*>(value) = static_cast<uint32_t>(std::max(0l, std::lround(number)));
            break;
        case PropertyKind::Int64:
            *static_cast<int64_t*>(value) = std::llround(number);
            break;
        case PropertyKind::Bool:
            *static_cast<bool*>(value) = number != 0.0f;
            break;
        default:
            return false;
        }
        object->OnPropertiesChanged();
        return true;
    }

    bool BoundProperty::WriteBool(bool flag) const
    {
        return WriteNumber(flag ? 1.0f : 0.0f);
    }

    bool BoundProperty::WriteString(const std::string& text) const
    {
        if (property->value.kind == PropertyKind::String)
        {
            *static_cast<std::string*>(property->GetValuePointer(object)) = text;
            object->OnPropertiesChanged();
            return true;
        }
        return WriteNumber(static_cast<float>(std::atof(text.c_str())));
    }

    bool BoundProperty::Trigger() const
    {
        if (property->value.kind == PropertyKind::Bool)
        {
            return WriteBool(true);
        }
        float current = 0.0f;
        if (ReadNumber(current))
        {
            return WriteNumber(current + 1.0f);
        }
        return false;
    }

    Core::Result<BoundProperty> ResolveBinding(const std::string& path, const BindingRoots& roots, BindingAccess access)
    {
        std::vector<std::string> segments = Core::Split(Core::Trim(path), '.');
        if (segments.size() < 2)
        {
            return Core::Error(Core::EngineText::BindingNeedsRootAndProperty, path, BindingRootList());
        }
        BindingRoot root;
        if (!FindBindingRoot(segments[0], root))
        {
            return Core::Error(Core::EngineText::BindingUnknownSource, segments[0], BindingRootList());
        }
        Reflection::EngineObject* current = roots.Get(root);
        Level::LevelObject* owner = Reflection::Cast<Level::LevelObject>(current);
        if (!current)
        {
            return BoundProperty{};
        }
        for (size_t i = 1; i < segments.size(); ++i)
        {
            const std::string& segment = segments[i];
            const Reflection::PropertyInfo* property = current->GetType().FindProperty(segment);
            if (property && i + 1 == segments.size())
            {
                if (!property->IsUIProperty())
                {
                    return Core::Error(Core::EngineText::BindingNotUIProperty, path);
                }
                if (access == BindingAccess::Write && !property->HasFlag(PropertyFlagUIWrite))
                {
                    return Core::Error(Core::EngineText::BindingReadOnly, path);
                }
                return BoundProperty{ current, property };
            }
            if (!owner)
            {
                return Core::Error(Core::EngineText::BindingCannotResolve, segment, path);
            }
            Reflection::EngineObject* next = nullptr;
            for (const auto& component : owner->GetComponents())
            {
                const Reflection::TypeInfo* type = &component->GetType();
                for (; type; type = type->GetBase())
                {
                    if (type->GetName() == segment)
                    {
                        break;
                    }
                }
                if (type)
                {
                    next = component.get();
                    break;
                }
            }
            if (!next)
            {
                return Core::Error(Core::EngineText::BindingNotPropertyOrComponent, segment, path);
            }
            current = next;
            owner = nullptr;
        }
        return Core::Error(Core::EngineText::BindingNoProperty, path);
    }

    TextTemplate TextTemplate::Parse(const std::string& text)
    {
        TextTemplate result;
        size_t position = 0;
        while (position < text.size())
        {
            size_t open = text.find('{', position);
            if (open == std::string::npos)
            {
                result.segments.push_back({ false, text.substr(position), -1 });
                break;
            }
            if (open > position)
            {
                result.segments.push_back({ false, text.substr(position, open - position), -1 });
            }
            size_t close = text.find('}', open);
            if (close == std::string::npos)
            {
                result.segments.push_back({ false, text.substr(open), -1 });
                break;
            }
            std::string inner = Core::Trim(text.substr(open + 1, close - open - 1));
            Segment segment;
            segment.isPath = true;
            size_t colon = inner.find(':');
            if (colon != std::string::npos)
            {
                segment.decimals = std::atoi(inner.substr(colon + 1).c_str());
                inner = Core::Trim(inner.substr(0, colon));
            }
            segment.text = inner;
            result.segments.push_back(segment);
            position = close + 1;
        }
        return result;
    }

    bool TextTemplate::HasBindings() const
    {
        for (const Segment& segment : segments)
        {
            if (segment.isPath)
            {
                return true;
            }
        }
        return false;
    }

    Core::Result<std::string> TextTemplate::Evaluate(const BindingRoots& roots) const
    {
        std::string result;
        for (const Segment& segment : segments)
        {
            if (!segment.isPath)
            {
                result += segment.text;
                continue;
            }
            Core::Result<BoundProperty> bound = ResolveBinding(segment.text, roots, BindingAccess::Read);
            if (!bound)
            {
                return bound.GetError();
            }
            result += *bound ? bound->Format(segment.decimals) : "";
        }
        return result;
    }
}
