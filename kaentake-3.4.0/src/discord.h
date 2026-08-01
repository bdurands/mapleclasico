#pragma once
#include <string>

namespace DiscordAPI {
    void Initialize(const char* clientId);
    void HandleUpdatePacket(void* pPacket);
    void UpdatePresence(const char* state, const char* details, const char* largeImageKey, const char* largeImageText);
    void RunCallbacks();
    void Shutdown();
}
