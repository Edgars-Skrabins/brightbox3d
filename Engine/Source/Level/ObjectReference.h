#pragma once

#include "Core/Guid.h"
#include "Level/Level.h"
#include "Reflection/TypeInfo.h"

namespace BrightBox3D::Level
{
    template <typename T>
    struct ObjectReference
    {
        static constexpr Reflection::PropertyKind ReflectionKind = Reflection::PropertyKind::ObjectReference;
        static const char* ReflectionReferenceType() { return T::StaticType().GetName().c_str(); }

        Core::Guid guid;

        ObjectReference() = default;
        ObjectReference(const T* object) : guid(object ? object->GetId() : Core::Guid{}) {}

        T* Get(const Level* level) const
        {
            if (!level || !guid.IsValid())
            {
                return nullptr;
            }
            LevelObjectInstance* object = level->FindObject(guid);
            return object && object->IsA(T::StaticType()) && !object->IsPendingDestroy() ? static_cast<T*>(object) : nullptr;
        }

        bool IsSet() const { return guid.IsValid(); }
        void Clear() { guid = Core::Guid{}; }
        bool operator==(const ObjectReference& other) const { return guid == other.guid; }
    };
}
