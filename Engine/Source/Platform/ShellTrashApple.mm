#include "Platform/Shell.h"

#include "Core/EngineText.h"

#import <Foundation/Foundation.h>

namespace BrightBox3D::Platform::Shell
{
    Core::Result<void> MoveToTrash(const std::filesystem::path& path)
    {
        NSURL* url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:path.string().c_str()]];
        NSError* error = nil;
        if (![[NSFileManager defaultManager] trashItemAtURL:url resultingItemURL:nil error:&error])
        {
            std::string reason = error ? std::string([[error localizedDescription] UTF8String]) : std::string();
            return Core::Error(Core::EngineText::TrashFailed, path.string(), reason);
        }
        return {};
    }
}
