#include "UI/Document.h"

#include "Assets/AssetDatabase.h"
#include "Core/EngineText.h"
#include "Core/Log.h"
#include "UI/DocumentBuilder.h"
#include "UI/DocumentPainter.h"
#include "UI/LayoutSync.h"
#include "UI/StyleResolver.h"
#include "UI/UIAssets.h"
#include "UI/WidgetController.h"

#include <pugixml.hpp>

namespace BrightBox3D::UI
{
    namespace
    {
        constexpr const char* DefaultStyleSheet = R"(
Screen { width: 100%; height: 100%; }
Label { color: #ffffff; }
Button { padding: 6px 14px; background-color: #3a3d42; border-radius: 4px; align-items: center; justify-content: center; text-align: center; }
Button:hover { background-color: #4a4e55; }
Button:active { background-color: #2a5c96; }
Button:disabled { opacity: 0.5; }
TextField { padding: 6px 8px; background-color: #1d1f22; border: 1px #44484f; border-radius: 3px; min-width: 140px; }
TextField:focus { border-color: #4a8be0; }
ProgressBar { height: 14px; min-width: 100px; background-color: #1d1f22; border-radius: 3px; }
Slider { height: 16px; min-width: 120px; background-color: #1d1f22; border-radius: 8px; }
Toggle { width: 18px; height: 18px; background-color: #1d1f22; border: 1px #5a5e65; border-radius: 3px; }
Toggle:checked { background-color: #2a5c96; }
Image { width: 64px; height: 64px; }
)";
    }

    Document::Document()
    {
        if (Core::Result<void> parsed = m_styles.Parse(DefaultStyleSheet); !parsed)
        {
            Core::Log::Error(parsed.GetError().GetText());
        }
    }

    Document::~Document()
    {
        for (const auto& controller : m_controllers)
        {
            controller->OnDestroyed();
        }
        m_controllers.clear();
        m_root.reset();
    }

    Core::Result<void> Document::LoadFromText(const std::string& markup)
    {
        pugi::xml_document document;
        pugi::xml_parse_result result = document.load_string(markup.c_str());
        if (!result)
        {
            return Core::Error(Core::EngineText::UIXmlError, result.description(), result.offset);
        }
        DocumentBuilder builder(*this, m_styles, m_controllers, m_dependencies);
        Core::Result<std::unique_ptr<Element>> root = builder.Build(document.document_element());
        if (!root)
        {
            return root.GetError();
        }
        m_root = std::move(*root);
        if (!m_root)
        {
            return Core::Error(Core::EngineText::UIEmptyScreen);
        }
        return {};
    }

    Core::Result<void> Document::LoadScreen(const Core::Guid& screen)
    {
        m_dependencies.insert(screen);
        auto asset = Assets::AssetDatabase::Get().Load<ScreenAsset>(screen);
        if (!asset)
        {
            return asset.GetError();
        }
        return LoadFromText(asset->GetText());
    }

    void Document::Update(const BindingRoots& roots, const UIInput& input, float width, float height, float deltaTime)
    {
        if (!m_root)
        {
            return;
        }
        m_bindings.SetRoots(roots);
        if (!m_created)
        {
            m_created = true;
            for (const auto& controller : m_controllers)
            {
                controller->OnCreated();
            }
        }
        m_bindings.Update(*m_root);
        StyleResolver::ComputeStyles(*m_root, m_styles);
        LayoutSync::Calculate(*m_root, width, height);
        m_input.Handle(*m_root, input, m_bindings);
        for (const auto& controller : m_controllers)
        {
            controller->OnUpdate(deltaTime);
        }
    }

    void Document::Draw(DrawList& drawList, RHI::Device& device, Rendering::GpuResourceCache& resources) const
    {
        if (m_root)
        {
            DocumentPainter::Draw(*m_root, drawList, device, resources);
        }
    }
}
