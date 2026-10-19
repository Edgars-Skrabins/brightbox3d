#pragma once

#include "Core/Guid.h"
#include "Scripts/ScriptTemplate.h"

#include <string>

namespace BrightBox3D::Editor
{
    class EditorApplication;

    struct CreateScriptRequest
    {
        ScriptTemplate scriptTemplate = ScriptTemplate::Component;
        std::string folder;
        Core::Guid targetObject;
    };

    class CreateScriptDialog
    {
    public:
        void Open(const CreateScriptRequest& request);
        void Draw(EditorApplication& application);

    private:
        void DrawTemplates();
        void DrawFolders(EditorApplication& application);
        bool Create(EditorApplication& application);

        CreateScriptRequest m_request;
        std::string m_name;
        std::string m_error;
        bool m_openRequested = false;
    };
}
