#include "Level/LevelOverrides.h"

#include "Level/Level.h"
#include "Level/LevelFileSchema.h"
#include "Level/LevelSerializer.h"
#include "Level/MissingComponent.h"
#include "Serialization/ObjectSerializer.h"

#include <iterator>

namespace BrightBox3D::Level
{
    using Serialization::GetString;
    using Serialization::JsonAllocator;
    using Serialization::JsonDocument;
    using Serialization::JsonValue;
    using Serialization::MakeString;

    namespace
    {
        constexpr Core::Guid TransformComponentId{ 0, 1 };

        enum class PseudoProperty
        {
            Name,
            Active,
            Enabled,
            Count
        };

        struct PseudoPropertyInfo
        {
            const char* key;
            bool (*equals)(const LevelObject& a, const LevelObjectComponent* componentA, const LevelObject& b, const LevelObjectComponent* componentB);
            void (*write)(const LevelObject& object, const LevelObjectComponent* component, JsonValue& out, JsonAllocator& allocator);
            void (*read)(LevelObject& object, LevelObjectComponent* component, const JsonValue& value);
        };

        constexpr PseudoPropertyInfo PseudoProperties[] = {
            {
                LevelFileSchema::PseudoName,
                [](const LevelObject& a, const LevelObjectComponent*, const LevelObject& b, const LevelObjectComponent*) { return a.GetName() == b.GetName(); },
                [](const LevelObject& object, const LevelObjectComponent*, JsonValue& out, JsonAllocator& allocator) { out = MakeString(object.GetName(), allocator); },
                [](LevelObject& object, LevelObjectComponent*, const JsonValue& value)
                {
                    if (value.IsString())
                    {
                        object.SetName(value.GetString());
                    }
                }
            },
            {
                LevelFileSchema::PseudoActive,
                [](const LevelObject& a, const LevelObjectComponent*, const LevelObject& b, const LevelObjectComponent*) { return a.IsActiveSelf() == b.IsActiveSelf(); },
                [](const LevelObject& object, const LevelObjectComponent*, JsonValue& out, JsonAllocator&) { out.SetBool(object.IsActiveSelf()); },
                [](LevelObject& object, LevelObjectComponent*, const JsonValue& value)
                {
                    if (value.IsBool())
                    {
                        object.SetActive(value.GetBool());
                    }
                }
            },
            {
                LevelFileSchema::PseudoEnabled,
                [](const LevelObject&, const LevelObjectComponent* componentA, const LevelObject&, const LevelObjectComponent* componentB) { return componentA && componentB && componentA->IsEnabled() == componentB->IsEnabled(); },
                [](const LevelObject&, const LevelObjectComponent* component, JsonValue& out, JsonAllocator&) { out.SetBool(component && component->IsEnabled()); },
                [](LevelObject&, LevelObjectComponent* component, const JsonValue& value)
                {
                    if (value.IsBool() && component)
                    {
                        component->SetEnabled(value.GetBool());
                    }
                }
            }
        };

        static_assert(std::size(PseudoProperties) == static_cast<size_t>(PseudoProperty::Count));

        const PseudoPropertyInfo& Info(PseudoProperty property)
        {
            return PseudoProperties[static_cast<size_t>(property)];
        }

        const PseudoPropertyInfo* FindPseudoProperty(const std::string& key)
        {
            for (const PseudoPropertyInfo& info : PseudoProperties)
            {
                if (key == info.key)
                {
                    return &info;
                }
            }
            return nullptr;
        }

        void VisitPseudoProperty(PseudoProperty property, const LevelObject& live, const LevelObjectComponent* liveComponent, const LevelObject& prototype, const LevelObjectComponent* prototypeComponent, const LevelOverrides::OverrideVisitor& visitor)
        {
            const PseudoPropertyInfo& info = Info(property);
            visitor(liveComponent, prototypeComponent, info.key, info.equals(live, liveComponent, prototype, prototypeComponent));
        }

        void VisitSerializedProperties(const Reflection::EngineObject& live, const Reflection::EngineObject& prototype, const LevelObjectComponent* liveComponent, const LevelObjectComponent* prototypeComponent, const LevelOverrides::OverrideVisitor& visitor)
        {
            for (const Reflection::PropertyInfo* property : live.GetType().GetAllProperties())
            {
                if (property->IsSerialized())
                {
                    visitor(liveComponent, prototypeComponent, property->name, Serialization::PropertyValuesEqual(*property, live, prototype));
                }
            }
        }

        JsonValue MakeOverrideEntry(const LevelObject& object, const LevelObjectComponent* component, const std::string& property, JsonAllocator& allocator)
        {
            JsonValue entry(rapidjson::kObjectType);
            entry.AddMember(LevelFileSchema::Target, MakeString(object.GetLink().sourceId.ToString(), allocator), allocator);
            std::string key = LevelOverrides::ComponentKey(object, component);
            if (!key.empty())
            {
                entry.AddMember(LevelFileSchema::Component, MakeString(key, allocator), allocator);
            }
            entry.AddMember(LevelFileSchema::Property, MakeString(property, allocator), allocator);
            JsonValue value;
            if (LevelOverrides::IsPseudoProperty(property))
            {
                LevelOverrides::WritePseudoProperty(object, component, property, value, allocator);
            }
            else
            {
                const Reflection::EngineObject& target = component ? static_cast<const Reflection::EngineObject&>(*component) : static_cast<const Reflection::EngineObject&>(object);
                Serialization::WriteProperty(*target.GetType().FindProperty(property), target, value, allocator);
            }
            entry.AddMember(LevelFileSchema::Value, value, allocator);
            return entry;
        }

        void ApplyOverride(LevelObject& object, const JsonValue& entry, const JsonValue& value)
        {
            std::string componentKey = GetString(entry, LevelFileSchema::Component);
            LevelObjectComponent* component = LevelOverrides::ResolveComponent(object, componentKey);
            if (!componentKey.empty() && !component)
            {
                return;
            }
            std::string property = GetString(entry, LevelFileSchema::Property);
            if (!property.empty() && LevelOverrides::IsPseudoProperty(property))
            {
                LevelOverrides::ReadPseudoProperty(object, component, property, value);
                return;
            }
            Reflection::EngineObject& target = component ? static_cast<Reflection::EngineObject&>(*component) : static_cast<Reflection::EngineObject&>(object);
            if (const Reflection::PropertyInfo* info = target.GetType().FindProperty(property))
            {
                Serialization::ReadProperty(*info, target, value);
                target.OnPropertiesChanged();
            }
        }
    }

    bool LevelOverrides::IsPseudoProperty(const std::string& property)
    {
        return property[0] == LevelFileSchema::PseudoPrefix;
    }

    bool LevelOverrides::PseudoPropertiesEqual(const LevelObject& a, const LevelObjectComponent* componentA, const LevelObject& b, const LevelObjectComponent* componentB, const std::string& property)
    {
        const PseudoPropertyInfo* info = FindPseudoProperty(property);
        return !info || info->equals(a, componentA, b, componentB);
    }

    void LevelOverrides::WritePseudoProperty(const LevelObject& object, const LevelObjectComponent* component, const std::string& property, JsonValue& out, JsonAllocator& allocator)
    {
        if (const PseudoPropertyInfo* info = FindPseudoProperty(property))
        {
            info->write(object, component, out, allocator);
        }
    }

    void LevelOverrides::ReadPseudoProperty(LevelObject& object, LevelObjectComponent* component, const std::string& property, const JsonValue& value)
    {
        if (const PseudoPropertyInfo* info = FindPseudoProperty(property))
        {
            info->read(object, component, value);
        }
    }

    std::string LevelOverrides::ComponentKey(const LevelObject& object, const LevelObjectComponent* component)
    {
        if (!component)
        {
            return {};
        }
        if (component == &object.GetTransform())
        {
            return LevelFileSchema::TransformComponentKey;
        }
        return component->GetLocalId().ToString();
    }

    LevelObjectComponent* LevelOverrides::ResolveComponent(LevelObject& object, const std::string& key)
    {
        if (key.empty())
        {
            return nullptr;
        }
        if (key == LevelFileSchema::TransformComponentKey)
        {
            return &object.GetTransform();
        }
        return object.FindComponentByLocalId(Core::Guid::Parse(key));
    }

    void LevelOverrides::ForEachOverrideCandidate(const LevelObject& live, const LevelObject& prototype, bool isRoot, const OverrideVisitor& visitor)
    {
        if (!isRoot)
        {
            VisitPseudoProperty(PseudoProperty::Name, live, nullptr, prototype, nullptr, visitor);
            VisitPseudoProperty(PseudoProperty::Active, live, nullptr, prototype, nullptr, visitor);
            for (const Reflection::PropertyInfo* property : live.GetTransform().GetType().GetAllProperties())
            {
                visitor(&live.GetTransform(), &prototype.GetTransform(), property->name, Serialization::PropertyValuesEqual(*property, live.GetTransform(), prototype.GetTransform()));
            }
        }
        if (&live.GetType() == &prototype.GetType() && live.GetMissingTypeName().empty())
        {
            VisitSerializedProperties(live, prototype, nullptr, nullptr, visitor);
        }
        for (const auto& component : live.GetComponents())
        {
            const LevelObjectComponent* other = prototype.FindComponentByLocalId(component->GetLocalId());
            if (!other || &other->GetType() != &component->GetType() || Reflection::Cast<MissingComponent>(component.get()))
            {
                continue;
            }
            VisitPseudoProperty(PseudoProperty::Enabled, live, component.get(), prototype, other, visitor);
            VisitSerializedProperties(*component, *other, component.get(), other, visitor);
        }
    }

    void LevelOverrides::WriteOverrides(const LevelObject& linkRoot, JsonValue& out, JsonAllocator& allocator)
    {
        out.SetArray();
        LevelObjectLibrary& library = LevelObjectLibrary::Get();
        for (const LevelObject* object : LevelObjectLinking::CollectLinked(linkRoot))
        {
            LevelObject* prototype = library.FindPrototypeNode(linkRoot.GetLink().asset, object->GetLink().sourceId);
            if (!prototype)
            {
                continue;
            }
            ForEachOverrideCandidate(*object, *prototype, object == &linkRoot, [&](const LevelObjectComponent* component, const LevelObjectComponent*, const std::string& property, bool equal)
            {
                if (!equal)
                {
                    out.PushBack(MakeOverrideEntry(*object, component, property, allocator), allocator);
                }
            });
        }
    }

    void LevelOverrides::ApplyOverrides(const JsonValue& overrides, const LevelObjectLinking::SourceIndex& bySource)
    {
        for (const JsonValue& entry : overrides.GetArray())
        {
            auto it = bySource.find(Core::Guid::Parse(GetString(entry, LevelFileSchema::Target)));
            const JsonValue* value = Serialization::FindMember(entry, LevelFileSchema::Value);
            if (it != bySource.end() && value)
            {
                ApplyOverride(*it->second, entry, *value);
            }
        }
    }

    std::vector<PropertyOverride> LevelSerializer::ComputeOverrides(const LevelObject& linkRoot)
    {
        std::vector<PropertyOverride> result;
        JsonDocument document;
        JsonValue overrides;
        LevelOverrides::WriteOverrides(linkRoot, overrides, document.GetAllocator());
        for (const JsonValue& entry : overrides.GetArray())
        {
            PropertyOverride item;
            item.target = Core::Guid::Parse(GetString(entry, LevelFileSchema::Target));
            std::string component = GetString(entry, LevelFileSchema::Component);
            item.component = component == LevelFileSchema::TransformComponentKey ? TransformComponentId : Core::Guid::Parse(component);
            item.property = GetString(entry, LevelFileSchema::Property);
            result.push_back(item);
        }
        return result;
    }

    bool LevelSerializer::IsPropertyOverridden(const LevelObject& object, const LevelObjectComponent* component, const std::string& property)
    {
        const LevelObject* linkRoot = object.GetLinkRoot();
        if (!linkRoot)
        {
            return false;
        }
        bool isRoot = linkRoot == &object;
        if (isRoot && (component == &object.GetTransform() || property == LevelFileSchema::PseudoName || property == LevelFileSchema::PseudoActive))
        {
            return false;
        }
        LevelObject* prototype = LevelObjectLibrary::Get().FindPrototypeNode(linkRoot->GetLink().asset, object.GetLink().sourceId);
        if (!prototype)
        {
            return false;
        }
        const LevelObjectComponent* other = nullptr;
        if (component)
        {
            other = component == &object.GetTransform() ? &prototype->GetTransform() : prototype->FindComponentByLocalId(component->GetLocalId());
            if (!other || &other->GetType() != &component->GetType())
            {
                return false;
            }
        }
        else if (&prototype->GetType() != &object.GetType())
        {
            return false;
        }
        if (LevelOverrides::IsPseudoProperty(property))
        {
            return !LevelOverrides::PseudoPropertiesEqual(object, component, *prototype, other, property);
        }
        const Reflection::EngineObject& live = component ? static_cast<const Reflection::EngineObject&>(*component) : static_cast<const Reflection::EngineObject&>(object);
        const Reflection::EngineObject& reference = other ? static_cast<const Reflection::EngineObject&>(*other) : static_cast<const Reflection::EngineObject&>(*prototype);
        const Reflection::PropertyInfo* info = live.GetType().FindProperty(property);
        return info && !Serialization::PropertyValuesEqual(*info, live, reference);
    }
}
