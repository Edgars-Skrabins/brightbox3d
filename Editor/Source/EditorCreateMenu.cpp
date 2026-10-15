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

#include <iterator>

namespace BrightBox3D::Editor
{
    namespace
    {
        constexpr float CameraSpawnHeight = 1.5f;

        template <Rendering::BuiltinPrimitive Primitive>
        const char* PrimitiveName()
        {
            return Rendering::BuiltinAssets::PrimitiveName(Primitive);
        }

        constexpr const char* (*PrimitiveNames[])() = {
            &PrimitiveName<Rendering::BuiltinPrimitive::Cube>,
            &PrimitiveName<Rendering::BuiltinPrimitive::Sphere>,
            &PrimitiveName<Rendering::BuiltinPrimitive::Plane>,
            &PrimitiveName<Rendering::BuiltinPrimitive::Quad>,
            &PrimitiveName<Rendering::BuiltinPrimitive::Cylinder>,
            &PrimitiveName<Rendering::BuiltinPrimitive::Capsule>
        };

        static_assert(std::size(PrimitiveNames) == static_cast<size_t>(Rendering::BuiltinPrimitive::Count));

        constexpr Rendering::BuiltinPrimitive MenuPrimitives[] = { Rendering::BuiltinPrimitive::Cube, Rendering::BuiltinPrimitive::Sphere, Rendering::BuiltinPrimitive::Capsule, Rendering::BuiltinPrimitive::Cylinder, Rendering::BuiltinPrimitive::Plane, Rendering::BuiltinPrimitive::Quad };

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
            for (Rendering::BuiltinPrimitive primitive : MenuPrimitives)
            {
                registry.Register({
                    .submenu = EditorText::CreateMenu3DObject,
                    .label = EditorText::PrimitiveLabel,
                    .labelSubject = PrimitiveNames[static_cast<size_t>(primitive)],
                    .icon = ObjectIcons::TypeIcon(Rendering::MeshRenderer::StaticType()),
                    .create = [primitive](const CreateMenuContext& context)
                    {
                        context.editor.GetObjectEditService().CreateModelObject(Rendering::BuiltinAssets::PrimitiveGuid(primitive), Rendering::BuiltinAssets::PrimitiveName(primitive), context.spawn, context.parent);
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
