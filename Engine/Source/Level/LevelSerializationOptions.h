#pragma once

namespace BrightBox3D::Level
{
    enum class TemplateAccess
    {
        Refuse,
        Allow
    };

    enum class LinkWriting
    {
        Preserve,
        Flatten
    };

    enum class NodeWriting
    {
        Level,
        Asset
    };

    enum class SaveScope
    {
        All,
        DirtyOnly
    };
}
