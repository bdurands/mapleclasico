#pragma once
#include <string>

// Player info updated whenever the server sends a DISCORD_UPDATE packet.
// Read by discord_ui.cpp to display in the in-game window.
extern std::string g_discord_playerName;
extern std::string g_discord_jobName;
extern std::string g_discord_mapName;
extern int         g_discord_level;

namespace DiscordAPI {
    void Initialize(const char* clientId);
    void HandleUpdatePacket(void* pPacket);
    void RunCallbacks();
    void Shutdown();
}
