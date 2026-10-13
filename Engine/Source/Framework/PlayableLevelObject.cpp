#include "Framework/PlayableLevelObject.h"

#include "Framework/Camera.h"
#include "Framework/PlayerController.h"

namespace BrightBox3D::Framework
{
    namespace
    {
        Camera* FindCameraBelow(const Level::LevelObject& object)
        {
            for (Level::LevelObject* child : object.GetChildren())
            {
                if (Camera* camera = Reflection::Cast<Camera>(child))
                {
                    return camera;
                }
                if (Camera* camera = FindCameraBelow(*child))
                {
                    return camera;
                }
            }
            return nullptr;
        }
    }

    Camera* PlayableLevelObject::FindCamera() const
    {
        return FindCameraBelow(*this);
    }

    void PlayableLevelObject::CarryReferences(Reflection::ReferenceCarrier& carrier)
    {
        carrier.Carry(m_controller);
    }
}
