#include "EditorApplication.h"

int main(int argc, char** argv)
{
    BrightBox3D::Editor::EditorApplication application;
    if (!application.Initialize(argc, argv))
    {
        return 1;
    }
    application.Run();
    application.Shutdown();
    return 0;
}
