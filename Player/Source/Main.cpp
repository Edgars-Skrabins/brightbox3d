#include "Core/CommandLine.h"
#include "Core/Log.h"
#include "PlayerApplication.h"
#include "PlayerCommandLine.h"

#include <optional>
#include <string>

using namespace BrightBox3D;

int main(int argc, char** argv)
{
    std::string commandLineError;
    std::optional<Player::PlayerOptions> options = Core::ParseCommandLine<Player::PlayerOptions>(argc, argv, Player::PlayerCommandLine, commandLineError);
    if (!options)
    {
        Core::Log::Error(commandLineError);
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
