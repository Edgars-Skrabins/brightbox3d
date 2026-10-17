#pragma once

#include "Assets/AssetType.h"
#include "Core/Guid.h"
#include "Core/Result.h"
#include "UI/Element.h"
#include "UI/Markup.h"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace pugi
{
    class xml_node;
}

namespace BrightBox3D::UI
{
    class Document;
    class StyleSheet;
    class WidgetController;

    class DocumentBuilder
    {
    public:
        DocumentBuilder(Document& document, StyleSheet& styles, std::vector<std::unique_ptr<WidgetController>>& controllers, std::unordered_set<Core::Guid>& dependencies);

        Core::Result<std::unique_ptr<Element>> Build(const pugi::xml_node& node);

    private:
        using MarkupValues = std::array<std::optional<std::string>, static_cast<size_t>(MarkupAttribute::Count)>;

        Core::Result<std::unique_ptr<Element>> BuildElement(const pugi::xml_node& node, int depth);
        void LoadStyleElement(const pugi::xml_node& node);
        bool LoadStyleSheet(const std::string& reference);
        void ReadAttributes(const pugi::xml_node& from, MarkupValues& values) const;
        void ApplyAttributes(Element& element, const MarkupValues& values, const pugi::xml_node& source);
        void AttachController(Element& element, const char* controllerName);
        Core::Guid ResolveAsset(const std::string& reference, Assets::AssetType expectedType);

        Document& m_document;
        StyleSheet& m_styles;
        std::vector<std::unique_ptr<WidgetController>>& m_controllers;
        std::unordered_set<Core::Guid>& m_dependencies;
    };
}
