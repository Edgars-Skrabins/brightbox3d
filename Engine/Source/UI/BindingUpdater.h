#pragma once

#include "UI/Binding.h"
#include "UI/Markup.h"

#include <string>
#include <unordered_set>

namespace BrightBox3D::UI
{
    class Element;

    class BindingUpdater
    {
    public:
        void SetRoots(const BindingRoots& roots) { m_roots = roots; }
        const BindingRoots& GetRoots() const { return m_roots; }

        void Update(Element& element);
        void Write(Element& element, BindingTarget target);

    private:
        void UpdateText(Element& element);
        void UpdateNumbers(Element& element);
        void UpdateVisibility(Element& element);
        template <typename Apply>
        void ReadNumber(Element& element, BindingTarget target, BindingAccess access, Apply apply);
        void ReportError(const Core::Error& error);

        BindingRoots m_roots;
        std::unordered_set<std::string> m_reportedErrors;
    };
}
