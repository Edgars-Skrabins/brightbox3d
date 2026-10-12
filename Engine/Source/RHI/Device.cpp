#include "RHI/Device.h"

namespace BrightBox3D::RHI
{
#if defined(__APPLE__)
    std::unique_ptr<Device> CreateMetalDevice();
#elif defined(_WIN32)
    std::unique_ptr<Device> CreateDX11Device();
#endif

    Backend Device::GetPlatformBackend()
    {
#if defined(_WIN32)
        return Backend::DX11;
#else
        return Backend::Metal;
#endif
    }

    std::unique_ptr<Device> Device::Create()
    {
#if defined(__APPLE__)
        return CreateMetalDevice();
#elif defined(_WIN32)
        return CreateDX11Device();
#else
        return nullptr;
#endif
    }
}
