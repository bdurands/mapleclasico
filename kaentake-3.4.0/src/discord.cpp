#include "discord.h"
#include "discord_rpc.h"
#include <time.h>
#include <string.h>
#include <string>

#pragma comment(lib, "discord-rpc.lib")

static int64_t startTimestamp = 0;

// ---------------------------------------------------------------------------
// Last-known player info — written in HandleUpdatePacket, read by discord_ui.cpp
// ---------------------------------------------------------------------------
std::string g_discord_playerName;
std::string g_discord_jobName;
std::string g_discord_mapName;
int         g_discord_level = 0;

namespace DiscordAPI {

    void Initialize(const char* clientId) {
        DiscordEventHandlers handlers;
        memset(&handlers, 0, sizeof(handlers));
        Discord_Initialize(clientId, &handlers, 1, nullptr);
        startTimestamp = time(0);
    }

    static void UpdatePresence(const char* state, const char* details,
                               const char* largeImageKey, const char* largeImageText) {
        DiscordRichPresence presence;
        memset(&presence, 0, sizeof(presence));
        presence.state          = state;
        presence.details        = details;
        presence.startTimestamp = startTimestamp;
        presence.largeImageKey  = largeImageKey;
        presence.largeImageText = largeImageText;
        Discord_UpdatePresence(&presence);
    }

    void HandleUpdatePacket(void* pPacket) {
        if (!pPacket) return;
        unsigned char*  base   = reinterpret_cast<unsigned char*>(pPacket);
        unsigned char*  data   = *reinterpret_cast<unsigned char**>(base + 0x8);
        unsigned int&   offset = *reinterpret_cast<unsigned int*>(base + 0x14);
        unsigned short  length = *reinterpret_cast<unsigned short*>(base + 0xC);

        // Skip 2-byte opcode since damageskin.cpp reset the offset
        offset += 2;

        auto readString = [&]() -> std::string {
            if (offset + 2 > length) return "";
            unsigned short strLen = *reinterpret_cast<unsigned short*>(data + offset);
            offset += 2;
            if (offset + strLen > length) return "";
            std::string str(reinterpret_cast<char*>(data + offset), strLen);
            offset += strLen;
            return str;
        };
        auto readInt = [&]() -> int {
            if (offset + 4 > length) return 0;
            int val = *reinterpret_cast<int*>(data + offset);
            offset += 4;
            return val;
        };

        std::string name    = readString();
        int         level   = readInt();
        std::string jobName = readString();
        std::string mapName = readString();

        // Store for the in-game Discord UI window
        g_discord_playerName = name;
        g_discord_level      = level;
        g_discord_jobName    = jobName;
        g_discord_mapName    = mapName;

        // Update Discord Rich Presence
        static std::string s_state, s_details;
        s_state   = "Explorando: " + mapName;
        s_details = name + " (Lv. " + std::to_string(level) + " " + jobName + ")";
        UpdatePresence(s_state.c_str(), s_details.c_str(), "logo", "EllinMS");
    }

    void RunCallbacks() {
        Discord_RunCallbacks();
    }

    void Shutdown() {
        Discord_Shutdown();
    }

} // namespace DiscordAPI
