#include "EditorCreateMenu.h"

#include "CreateMenuRegistry.h"
#include "EditorContext.h"
#include "Framework/Camera.h"
#include "Framework/PlayerStart.h"
#include "Labels.h"
#include "ObjectIcons.h"
#include "Rendering/BuiltinAssets.h"
#include "Rendering/Light.h"
#include "Rendering/MeshRenderer.h"

namespace BrightBox3D::Editor
{
    namespace
    {
        constexpr float CameraSpawnHeight = 1.5f;

        struct Primitive
        {
            EditorText name;
            Core::Guid (*model)();
        };

        struct LightEntry
        {
            EditorText name;
            Rendering::LightType type;
        };

        void RegisterEmptyObject(CreateMenuRegistry& registry)
        {
            registry.Register({
                .label = EditorText::EmptyLevelObject,
                .labelSubject = &TypeName<Level::LevelObject>,
                .icon = ObjectIcons::TypeIcon(Level::LevelObject::StaticType()),
                .create = [](const CreateMenuContext& context)
                {
                    context.editor.GetObjectEditService().CreateObject(Level::LevelObject::StaticType(), Level::LevelObject::StaticType().GetName(), context.parent);
                },
            });
        }

        void RegisterPrimitives(CreateMenuRegistry& registry)
        {
            const Primitive primitives[] = {
                { EditorText::PrimitiveCube, &Rendering::BuiltinAssets::Cube },
                { EditorText::PrimitiveSphere, &Rendering::BuiltinAssets::Sphere },
                { EditorText::PrimitiveCapsule, &Rendering::BuiltinAssets::Capsule },
                { EditorText::PrimitiveCylinder, &Rendering::BuiltinAssets::Cylinder },
                { EditorText::PrimitivePlane, &Rendering::BuiltinAssets::Plane },
                { EditorText::PrimitiveQuad, &Rendering::BuiltinAssets::Quad }
            };
            for (const Primitive& primitive : primitives)
            {
                registry.Register({
                    .submenu = EditorText::CreateMenu3DObject,
                    .label = primitive.name,
                    .icon = ObjectIcons::TypeIcon(Rendering::MeshRenderer::StaticType()),
                    .create = [primitive](const CreateMenuContext& context)
                    {
                        context.editor.GetObjectEditService().CreateModelObject(primitive.model(), Text(primitive.name), context.spawn, context.parent);
                    },
                });
            }
        }

        void RegisterLights(CreateMenuRegistry& registry)
        {
            const LightEntry lights[] = {
                { EditorText::DirectionalLight, Rendering::LightType::Directional },
                { EditorText::PointLight, Rendering::LightType::Point },
                { EditorText::SpotLight, Rendering::LightType::Spot }
            };
            for (const LightEntry& light : lights)
            {
                registry.Register({
                    .submenu = EditorText::CreateMenuLight,
                    .label = light.name,
                    .icon = ObjectIcons::TypeIcon(Rendering::Light::StaticType()),
                    .create = [type = light.type](const CreateMenuContext& context)
                    {
                        context.editor.GetObjectEditService().CreateLight(type);
                    },
                });
            }
        }

        void RegisterPlacements(CreateMenuRegistry& registry)
        {
            registry.RegisterPlacement(Framework::PlayerStart::StaticType(), [](Level::LevelObject& object, const CreateMenuContext& context)
            {
                object.GetTransform().SetPosition({ context.spawn.x, 0.0f, context.spawn.z });
            });
            registry.RegisterPlacement(Framework::Camera::StaticType(), [](Level::LevelObject& object, const CreateMenuContext& context)
            {
                object.GetTransform().SetPosition(context.spawn + Math::Vector3(0.0f, CameraSpawnHeight, 0.0f));
            });
        }
    }

    void RegisterEditorCreateMenu()
    {
        CreateMenuRegistry& registry = CreateMenuRegistry::Get();
        registry.Clear();
        RegisterEmptyObject(registry);
        RegisterPrimitives(registry);
        RegisterLights(registry);
        RegisterPlacements(registry);
    }
}
