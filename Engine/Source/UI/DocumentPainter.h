#pragma once

namespace BrightBox3D::RHI
{
    class Device;
}

namespace BrightBox3D::Rendering
{
    class GpuResourceCache;
}

namespace BrightBox3D::UI
{
    class DrawList;
    class Element;
}

namespace BrightBox3D::UI::DocumentPainter
{
    void Draw(const Element& root, DrawList& drawList, RHI::Device& device, Rendering::GpuResourceCache& resources);
}
