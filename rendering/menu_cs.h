#pragma once
// Azure Fortnite Private -- refined overlay menu.
// Ported 1:1 from the "CS2 external" reference menu (Menu.h / Menu.cpp / fonts.h)
// and extended with the Fortnite feature set + configurable menu/exit hotkeys.
//
// The full implementation (UI, config save/load, key binding) lives in
// src/core/Menu.cpp. State shared across translation units is inline here so
// the overlay loop, the discord bridge and the menu itself all see one Config.

struct ImFont;

#include <thirdparty/imgui/imgui.h>
#include <thirdparty/imgui/imgui_impl_dx11.h>
#include <d3d11.h>
#include <Windows.h>

namespace Menu {

    enum BindMode {
        BindMode_AlwaysOn = 0,
        BindMode_Hold = 1
    };

    struct KeyBind {
        int mode = BindMode_AlwaysOn;
        int key = 0;
    };

    struct Config {
        // Aim
        bool aimEnabled = false;
        float aimFov = 120.0f;
        float aimSmooth = 1.0f;
        int aimTargetBone = 0;
        KeyBind aimBind{ BindMode_Hold, 'Q' };
        bool aimShowFov = true;
        float aimFovColor[4] = { 0.95f, 0.83f, 0.46f, 0.55f };
        bool aimVisibleOnly = true;

        // Trigger
        bool triggerEnabled = false;
        KeyBind triggerBind{ BindMode_Hold, 'F' };
        bool triggerVisibleOnly = true;
        bool triggerAimAssist = true;
        int triggerTargetBone = 0;
        float triggerTapTimeMs = 1.0f;
        float triggerCooldownMs = 90.0f;

        // ESP -- Enemy
        bool espEnemyBox = false;
        bool espEnemyName = false;
        bool espEnemyHealth = false;
        bool espEnemyHpText = false;
        bool espEnemyTracer = false;
        bool espEnemySkeleton = false;
        bool espEnemyWeaponText = false;
        bool espEnemyWeaponIcon = false;
        float espEnemyColor[4] = { 0.93f, 0.77f, 0.36f, 1.0f };
        float espEnemyTracerColor[4] = { 0.96f, 0.60f, 0.28f, 1.0f };
        float espEnemySkeletonColor[4] = { 1.0f, 0.88f, 0.58f, 1.0f };
        float espEnemyNameColor[4] = { 0.93f, 0.77f, 0.36f, 1.0f };
        float espEnemyHealthColor[4] = { 0.40f, 0.88f, 0.40f, 1.0f };
        float espEnemyWeaponColor[4] = { 0.93f, 0.77f, 0.36f, 1.0f };

        // ESP -- Team
        bool espTeamBox = false;
        bool espTeamName = false;
        bool espTeamHealth = false;
        bool espTeamHpText = false;
        bool espTeamTracer = false;
        bool espTeamSkeleton = false;
        bool espTeamWeaponText = false;
        bool espTeamWeaponIcon = false;
        float espTeamColor[4] = { 0.24f, 0.62f, 0.96f, 1.0f };
        float espTeamTracerColor[4] = { 0.18f, 0.80f, 0.88f, 1.0f };
        float espTeamSkeletonColor[4] = { 0.52f, 0.86f, 1.0f, 1.0f };
        float espTeamNameColor[4] = { 0.24f, 0.62f, 0.96f, 1.0f };
        float espTeamHealthColor[4] = { 0.40f, 0.88f, 0.40f, 1.0f };
        float espTeamWeaponColor[4] = { 0.24f, 0.62f, 0.96f, 1.0f };

        // ESP -- shared / advanced
        int espBoxType = 1;                      // 0 full box, 1 corner box
        float espBoxThickness = 1.0f;
        bool espBoxOutline = false;
        float espCornerBoxThickness = 1.0f;
        bool espCornerBoxOutline = false;
        float espSkeletonThickness = 1.0f;
        bool espSkeletonOutline = false;
        float espTracerThickness = 1.0f;
        bool espTracerOutline = false;
        float espTextSize = 15.0f;
        bool espTextOutline = true;
        bool espShowDistance = true;
        bool espShowPlatform = false;
        bool espShowRank = false;
        bool espShowAmmo = false;
        bool espFovTriangles = false;
        bool espFovTriangleFilled = true;
        bool espFovTriangleOutline = false;
        bool espFovTriangleDynamic = true;
        float espFovTriangleSize = 12.0f;
        float espFovTriangleOffset = 18.0f;
        float espFovTriangleWidth = 2.4f;

        // Loot ESP
        bool lootChests = false;
        float lootChestColor[4] = { 1.0f, 0.82f, 0.35f, 1.0f };
        bool lootPickups = false;
        bool lootDrops = false;
        float lootMaxDist = 150.0f;
        bool lootShowDist = true;

        // Misc
        bool watermarkEnabled = true;
        int menuKey = VK_INSERT;
        int exitKey = VK_ESCAPE;

        // Rage exploits
        bool rageWalls = false;
        bool rageSpeed = false;
        float rageSpeedVal = 1.5f;
        bool rageInfAmmo = false;
        bool rageNoReload = false;
        bool rageRapidFire = false;
        bool rageFovOn = false;
        float rageFovVal = 110.0f;
        bool rageSpin = false;
        int rageZoomKey = 0;
        float rageZoomFov = 30.0f;
        int ragePanicKey = 0;
    };

    // Shared singleton state -- single definition across all translation units.
    inline Config g_config;
    inline ImFont* g_menuFont = nullptr;
    inline void* g_renderDevice = nullptr;
    inline ID3D11ShaderResourceView* g_avatarSrv = nullptr;
    inline constexpr float kMenuFontSize = 21.0f;
    inline constexpr float kEspFontSize = 16.0f;

    void Initialize();
    bool Render();
    bool IsBindActive(const KeyBind& bind);
    void SetDevice(void* device);
    void ShutdownUi();
    void sync();
    void ReloadAutosave();
    Config& GetConfig();
    ImFont* GetEspFont();
    float GetEspFontSize();
    int GetMenuKey();
    int GetExitKey();
    void SaveAutosave();

    void SetEspFont(ImFont* font);

    namespace detail {
        struct Idle { char _; };
    }
}