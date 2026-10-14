#pragma once

#include <string>

namespace BrightBox3D::Editor
{
    class LevelService;

    class NewLevelDialog
    {
    public:
        void Open();
        void Draw(LevelService& levels);

    private:
        std::string m_name;
        bool m_openRequested = false;
    };
}
