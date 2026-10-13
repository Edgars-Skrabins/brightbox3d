#pragma once

#include "Core/Guid.h"
#include "Reflection/EngineObject.h"

#include <string>

namespace BrightBox3D::Runtime
{
    BrightClass()
    class ProjectSettings : public Reflection::EngineObject
    {
        BrightBody()
    public:
        BrightProperty()
        std::string name;

        BrightProperty()
        std::string engineVersion;

        BrightProperty()
        std::string gameModule;

        BrightProperty()
        Core::Guid startupLevel;

        BrightProperty()
        float fixedTimeStep = 1.0f / 60.0f;

        BrightProperty()
        bool autoHotReload = true;
    };
}
