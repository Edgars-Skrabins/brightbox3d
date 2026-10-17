#include "Core/CommandLine.h"
#include "Core/Log.h"
#include "PlayerApplication.h"
#include "PlayerCommandLine.h"

#include <string>

using namespace BrightBox3D;

int main(int argc, char** argv)
{
    Core::Result<Player::PlayerOptions> options = Core::ParseCommandLine<Player::PlayerOptions>(argc, argv, Player::PlayerCommandLine);
    if (!options)
    {
        Core::Log::Error(options.GetError().GetText());
        Core::Log::Info(Core::CommandLineUsage<Player::PlayerOptions>(std::filesystem::path(argv[0]).filename().string(), Player::PlayerCommandLine));
        return 1;
    }
    Player::PlayerApplication application;
    if (!application.Initialize(*options))
    {
        return 1;
    }
    application.Run();
    application.Shutdown();
    return 0;
}
