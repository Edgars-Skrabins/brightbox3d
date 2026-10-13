#pragma once

#include "Core/Icon.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace BrightBox3D::Assets
{
    enum class AssetType : uint8_t;
}

namespace BrightBox3D::Reflection
{
    class EngineObject;
    class TypeInfo;

    template <typename T>
    struct TypeBuilder;

    enum class PropertyKind
    {
        Bool,
        Int32,
        UInt32,
        Int64,
        Float,
        Double,
        String,
        Vector2,
        Vector3,
        Vector4,
        Quaternion,
        Color,
        Guid,
        Enum,
        AssetReference,
        ObjectReference,
        ClassReference,
        Array
    };

    enum PropertyFlags : uint32_t
    {
        PropertyFlagNone = 0,
        PropertyFlagEditAnywhere = 1u << 0,
        PropertyFlagEditDefaultsOnly = 1u << 1,
        PropertyFlagEditInstanceOnly = 1u << 2,
        PropertyFlagVisibleAnywhere = 1u << 3,
        PropertyFlagHidden = 1u << 4,
        PropertyFlagUIRead = 1u << 5,
        PropertyFlagUIWrite = 1u << 6,
        PropertyFlagTransient = 1u << 7,
        PropertyFlagEditorVisible = PropertyFlagEditAnywhere | PropertyFlagEditDefaultsOnly | PropertyFlagEditInstanceOnly | PropertyFlagVisibleAnywhere
    };

    enum TypeFlags : uint32_t
    {
        TypeFlagNone = 0,
        TypeFlagAbstract = 1u << 0,
        TypeFlagHiddenInEditor = 1u << 1
    };

    struct EnumEntry
    {
        std::string name;
        int64_t value = 0;
    };

    struct EnumInfo
    {
        std::string name;
        std::vector<EnumEntry> entries;
        size_t size = 4;

        const EnumEntry* FindByName(const std::string& entryName) const;
        const EnumEntry* FindByValue(int64_t entryValue) const;
    };

    struct ValueDescriptor
    {
        PropertyKind kind = PropertyKind::Int32;
        const EnumInfo* enumInfo = nullptr;
        std::string referenceType;
        Assets::AssetType assetType{};
        const TypeInfo& (*baseType)() = nullptr;
        size_t size = 0;
    };

    struct ArrayAccess
    {
        size_t (*size)(const void* array) = nullptr;
        void (*resize)(void* array, size_t count) = nullptr;
        void* (*element)(void* array, size_t index) = nullptr;
    };

    struct PropertyInfo
    {
        std::string name;
        std::string displayName;
        std::string category;
        uint32_t flags = PropertyFlagNone;
        ValueDescriptor value;
        ValueDescriptor element;
        ArrayAccess array;
        void* (*access)(EngineObject* object) = nullptr;
        const TypeInfo* owner = nullptr;

        void* GetValuePointer(EngineObject* object) const { return access(object); }
        const void* GetValuePointer(const EngineObject* object) const { return access(const_cast<EngineObject*>(object)); }

        template <typename T>
        T& GetValue(EngineObject* object) const { return *static_cast<T*>(access(object)); }

        template <typename T>
        const T& GetValue(const EngineObject* object) const { return *static_cast<const T*>(GetValuePointer(object)); }

        bool HasFlag(uint32_t flag) const { return (flags & flag) != 0; }
        bool IsUIProperty() const { return HasFlag(PropertyFlagUIRead | PropertyFlagUIWrite); }
        bool IsSerialized() const { return !HasFlag(PropertyFlagTransient); }
    };

    class TypeInfo
    {
    public:
        using Factory = EngineObject* (*)();
        using Builder = void (*)(TypeInfo&);

        TypeInfo(std::string name, std::string moduleName, const TypeInfo* base, size_t size, uint32_t flags, Factory factory, Builder builder);

        const std::string& GetName() const { return m_name; }
        const TypeInfo* GetBase() const { return m_base; }
        size_t GetSize() const { return m_size; }
        uint32_t GetFlags() const { return m_flags; }
        bool IsAbstract() const { return (m_flags & TypeFlagAbstract) != 0 || m_factory == nullptr; }
        bool IsA(const TypeInfo& other) const;
        EngineObject* CreateInstance() const;

        const std::vector<PropertyInfo>& GetOwnProperties() const { return m_properties; }
        std::vector<const PropertyInfo*> GetAllProperties() const;
        const PropertyInfo* FindProperty(const std::string& propertyName) const;

        void AddProperty(PropertyInfo property);
        const std::string& GetModuleName() const { return m_moduleName; }
        void SetCategory(std::string category) { m_category = std::move(category); }
        const std::string& GetCategory() const { return m_category; }
        void SetIcon(Core::Icon icon) { m_icon = icon; }
        Core::Icon GetIcon() const { return m_icon; }

    private:
        std::string m_name;
        const TypeInfo* m_base = nullptr;
        size_t m_size = 0;
        uint32_t m_flags = TypeFlagNone;
        Factory m_factory = nullptr;
        std::vector<PropertyInfo> m_properties;
        std::string m_moduleName;
        std::string m_category;
        Core::Icon m_icon = Core::Icon::None;
    };
}
