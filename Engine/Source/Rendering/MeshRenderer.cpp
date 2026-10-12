#include "Rendering/MeshRenderer.h"

namespace BrightBox3D::Rendering
{
    void MeshRenderer::SetMaterial(size_t slot, const Assets::AssetReference<MaterialAsset>& material)
    {
        if (m_materials.size() <= slot)
        {
            m_materials.resize(slot + 1);
        }
        m_materials[slot] = material;
    }
}
