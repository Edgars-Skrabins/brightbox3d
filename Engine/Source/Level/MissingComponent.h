#pragma once

#include "Level/LevelObjectComponent.h"

#include <string>

namespace BrightBox3D::Level
{
    BrightClass()
    class MissingComponent : public LevelObjectComponent
    {
        BrightBody()
    public:
        const std::string& GetMissingTypeName() const { return m_missingTypeName; }
        const std::string& GetData() const { return m_data; }
        void SetMissing(const std::string& typeName, const std::string& data)
        {
            m_missingTypeName = typeName;
            m_data = data;
        }

    private:
        std::string m_missingTypeName;
        std::string m_data;
    };
}
