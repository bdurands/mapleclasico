#include "discord.h"
#include "discord_rpc.h"
#include <time.h>
#include <string.h>

#pragma comment(lib, "discord-rpc.lib")

static int64_t startTimestamp = 0;

namespace DiscordAPI {
    void Initialize(const char* clientId) {
        DiscordEventHandlers handlers;
        memset(&handlers, 0, sizeof(handlers));
        Discord_Initialize(clientId, &handlers, 1, nullptr);
        startTimestamp = time(0);
    }

    void UpdatePresence(const char* state, const char* details, const char* largeImageKey, const char* largeImageText) {
        DiscordRichPresence discordPresence;
        memset(&discordPresence, 0, sizeof(discordPresence));
        discordPresence.state = state;
        discordPresence.details = details;
        discordPresence.startTimestamp = startTimestamp;
        discordPresence.largeImageKey = largeImageKey;
        discordPresence.largeImageText = largeImageText;
        Discord_UpdatePresence(&discordPresence);
    }

    void HandleUpdatePacket(void* pPacket) {
        if (!pPacket) return;
        unsigned char* base = reinterpret_cast<unsigned char*>(pPacket);
        unsigned char* data = *reinterpret_cast<unsigned char**>(base + 0x8);
        unsigned int& offset = *reinterpret_cast<unsigned int*>(base + 0x14);
        unsigned short length = *reinterpret_cast<unsigned short*>(base + 0xC);

        auto readString = [&]() -> std::string {
            if (offset + 2 > length) return "";
            unsigned short strLen = *reinterpret_cast<unsigned short*>(data + offset);
            offset += 2;
            if (offset + strLen > length) return "";
            std::string str((char*)(data + offset), strLen);
            offset += strLen;
            return str;
        };

        auto readInt = [&]() -> int {
            if (offset + 4 > length) return 0;
            int val = *reinterpret_cast<int*>(data + offset);
            offset += 4;
            return val;
        };

        std::string name = readString();
        int level = readInt();
        std::string jobName = readString();
        std::string mapName = readString();

        static std::string s_state;
        static std::string s_details;

        s_state = "Explorando: " + mapName;
        s_details = name + " (Lv. " + std::to_string(level) + " " + jobName + ")";

        UpdatePresence(s_state.c_str(), s_details.c_str(), "logo", "EllinMS");
    }

    void RunCallbacks() {
        Discord_RunCallbacks();
    }

    void Shutdown() {
        Discord_Shutdown();
    }
}
