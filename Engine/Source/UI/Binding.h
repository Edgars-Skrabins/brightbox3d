#pragma once

#include "Reflection/Object.h"

#include <string>
#include <vector>

namespace BrightBox3D::Level
{
    class Level;
    class LevelObjectInstance;
}

namespace BrightBox3D::UI
{
    struct BindingRoots
    {
        Level::Level* level = nullptr;
        Level::LevelObjectInstance* player = nullptr;
        Reflection::Object* gameMode = nullptr;
    };

    struct BoundProperty
    {
        Reflection::Object* object = nullptr;
        const Reflection::PropertyInfo* property = nullptr;

        explicit operator bool() const { return object && property; }
        std::string Format(int decimals) const;
        bool ReadNumber(float& value) const;
        bool ReadBool(bool& value) const;
        bool WriteNumber(float value) const;
        bool WriteBool(bool value) const;
        bool WriteString(const std::string& value) const;
        bool Trigger() const;
    };

    enum class BindingAccess
    {
        Read,
        Write
    };

    BoundProperty ResolveBinding(const std::string& path, const BindingRoots& roots, BindingAccess access, std::string& error);

    struct TextTemplate
    {
        struct Segment
        {
            bool isPath = false;
            std::string text;
            int decimals = -1;
        };

        std::vector<Segment> segments;

        static TextTemplate Parse(const std::string& text);
        bool HasBindings() const;
        bool IsSingleBinding() const { return segments.size() == 1 && segments[0].isPath; }
        std::string Evaluate(const BindingRoots& roots, std::string& error) const;
    };
}
