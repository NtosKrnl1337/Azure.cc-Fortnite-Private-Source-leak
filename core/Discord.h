#pragma once

#include <d3d11.h>
#include <atomic>
#include <mutex>
#include <vector>
#include <string>

namespace Discord {
    extern std::atomic<bool> have_account;
    extern std::atomic<bool> need_avatar;
    extern std::mutex mutex;
    extern char name[64];
    extern std::vector<unsigned char> avatar_png;

    void StartWorker();
    void Shutdown();

    ID3D11ShaderResourceView* CreateAvatarTexture(ID3D11Device* device, bool circle_mask);
}

struct DiscordState {
    bool have_account;
    bool need_avatar;
};

extern DiscordState g_discord_state;

void discord_refresh_avatar();
const char* discord_display_name();