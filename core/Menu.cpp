#include <src/rendering/menu_cs.h>
#include <src/cache/index.h>
#include <src/cache/loot.h>
#include <src/aim/aimbot.h>
#include <src/core/Discord.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <locale>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

    struct ConfigUiState {
        std::vector<std::string> configNames;
        int selectedIndex = -1;
        char newConfigName[64] = "default";
        std::string status;
    };

    struct EspSettingsRef {
        bool& box;
        bool& name;
        bool& health;
        bool& hpText;
        bool& tracer;
        bool& skeleton;
        bool& weaponText;
        bool& weaponIcon;
        float* boxColor;
        float* nameColor;
        float* healthColor;
        float* tracerColor;
        float* skeletonColor;
        float* weaponColor;
    };

    constexpr const char* kBindModeItems[] = {
        "Always On",
        "Hold Key"
    };

    constexpr const char* kAimPointItems[] = {
        "Head",
        "Body"
    };

    constexpr const char* kBoxTypeItems[] = {
        "Full box",
        "Corner box"
    };

    const char* AutosaveName() { return "autosave"; }

    std::filesystem::path GetProjectRoot() {
        char modulePath[MAX_PATH]{};
        GetModuleFileNameA(nullptr, modulePath, MAX_PATH);
        std::filesystem::path exePath(modulePath);
        std::filesystem::path exeDir = exePath.parent_path();
        return exeDir.filename() == "build" ? exeDir.parent_path() : exeDir;
    }

    std::filesystem::path GetConfigDirectory() {
        const std::filesystem::path cfgDir = GetProjectRoot() / "cfg";
        std::filesystem::create_directories(cfgDir);
        return cfgDir;
    }

    std::filesystem::path GetConfigPath(const std::string& configName) {
        return GetConfigDirectory() / (configName + ".cfg");
    }

    std::string Trim(const std::string& value) {
        size_t start = 0;
        while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0)
            ++start;

        size_t end = value.size();
        while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0)
            --end;

        return value.substr(start, end - start);
    }

    std::string SanitizeConfigName(const char* rawName) {
        std::string sanitized;
        if (!rawName)
            return sanitized;

        for (const unsigned char ch : std::string(rawName)) {
            if (std::isalnum(ch) != 0 || ch == ' ' || ch == '_' || ch == '-')
                sanitized.push_back(static_cast<char>(ch));
        }

        return Trim(sanitized);
    }

    std::filesystem::path GetConfigDir() { return GetConfigDirectory(); }

} // namespace

namespace Menu {

    static ConfigUiState& ConfigUi() {
        static ConfigUiState ui;
        return ui;
    }

    void SetDevice(void* device) { g_renderDevice = device; }
    void SetEspFont(ImFont* font) { g_esp_font = font; }

    bool IsBindActive(const KeyBind& bind) {
        if (bind.mode == BindMode_AlwaysOn)
            return true;
        return bind.key != 0 && (GetAsyncKeyState(bind.key) & 0x8000) != 0;
    }

    int GetMenuKey() { return g_config.menuKey != 0 ? g_config.menuKey : VK_INSERT; }
    int GetExitKey() { return g_config.exitKey != 0 ? g_config.exitKey : VK_ESCAPE; }
    Config& GetConfig() { return g_config; }
    ImFont* GetEspFont() { return g_menuFont ? g_menuFont : g_esp_font; }
    float GetEspFontSize() { return kEspFontSize; }

    void ShutdownUi() {
        if (g_avatarSrv) {
            g_avatarSrv->Release();
            g_avatarSrv = nullptr;
        }
    }

    // ---- config serialization ---------------------------------------------

    namespace {

        void RefreshConfigList(const std::string& preferredSelection = std::string()) {
            auto& ui = ConfigUi();
            std::string selection = preferredSelection;
            if (selection.empty() && ui.selectedIndex >= 0 && ui.selectedIndex < static_cast<int>(ui.configNames.size()))
                selection = ui.configNames[ui.selectedIndex];

            ui.configNames.clear();
            for (const auto& entry : std::filesystem::directory_iterator(GetConfigDirectory())) {
                if (!entry.is_regular_file() || entry.path().extension() != ".cfg")
                    continue;
                ui.configNames.push_back(entry.path().stem().string());
            }

            std::sort(ui.configNames.begin(), ui.configNames.end());

            ui.selectedIndex = -1;
            for (int i = 0; i < static_cast<int>(ui.configNames.size()); ++i) {
                if (ui.configNames[i] == selection) {
                    ui.selectedIndex = i;
                    break;
                }
            }

            if (ui.selectedIndex == -1 && !ui.configNames.empty())
                ui.selectedIndex = 0;
        }

        void WriteBool(std::ostream& output, const char* key, bool value) {
            output << key << '=' << (value ? 1 : 0) << '\n';
        }

        void WriteInt(std::ostream& output, const char* key, int value) {
            output << key << '=' << value << '\n';
        }

        void WriteFloat(std::ostream& output, const char* key, float value) {
            output << key << '=' << value << '\n';
        }

        void WriteColor(std::ostream& output, const char* key, const float color[4]) {
            output << key << '='
                << color[0] << ','
                << color[1] << ','
                << color[2] << ','
                << color[3] << '\n';
        }

        void WriteBind(std::ostream& output, const char* key, const KeyBind& bind) {
            output << key << '=' << bind.mode << ',' << bind.key << '\n';
        }

        bool ParseIntegerValue(const std::string& text, int& outValue) {
            std::istringstream stream(text);
            stream.imbue(std::locale::classic());
            stream >> outValue;
            return !stream.fail() && stream.eof();
        }

        bool ParseBool(const std::unordered_map<std::string, std::string>& values, const char* key, bool& outValue) {
            const auto it = values.find(key);
            if (it == values.end())
                return false;

            int parsed = 0;
            if (!ParseIntegerValue(it->second, parsed))
                return false;

            outValue = parsed != 0;
            return true;
        }

        bool ParseInt(const std::unordered_map<std::string, std::string>& values, const char* key, int& outValue) {
            const auto it = values.find(key);
            if (it == values.end())
                return false;
            return ParseIntegerValue(it->second, outValue);
        }

        bool ParseFloat(const std::unordered_map<std::string, std::string>& values, const char* key, float& outValue) {
            const auto it = values.find(key);
            if (it == values.end())
                return false;

            std::istringstream stream(it->second);
            stream.imbue(std::locale::classic());
            stream >> outValue;
            return !stream.fail();
        }

        bool ParseColor(const std::unordered_map<std::string, std::string>& values, const char* key, float color[4]) {
            const auto it = values.find(key);
            if (it == values.end())
                return false;

            std::istringstream stream(it->second);
            stream.imbue(std::locale::classic());
            char separator1 = 0;
            char separator2 = 0;
            char separator3 = 0;
            stream >> color[0] >> separator1 >> color[1] >> separator2 >> color[2] >> separator3 >> color[3];
            return !stream.fail() &&
                separator1 == ',' &&
                separator2 == ',' &&
                separator3 == ',' &&
                stream.eof();
        }

        bool ParseBind(const std::unordered_map<std::string, std::string>& values, const char* key, KeyBind& bind) {
            const auto it = values.find(key);
            if (it == values.end())
                return false;

            std::istringstream stream(it->second);
            char separator = 0;
            stream >> bind.mode >> separator >> bind.key;
            return !stream.fail() && separator == ',' && stream.eof();
        }

        void SaveAllConfig(std::ostream& output) {
            output << "version=4\n";

            WriteBool(output, "aimEnabled", g_config.aimEnabled);
            WriteFloat(output, "aimFov", g_config.aimFov);
            WriteFloat(output, "aimSmooth", g_config.aimSmooth);
            WriteInt(output, "aimTargetBone", g_config.aimTargetBone);
            WriteBind(output, "aimBind", g_config.aimBind);
            WriteBool(output, "aimShowFov", g_config.aimShowFov);
            WriteColor(output, "aimFovColor", g_config.aimFovColor);
            WriteBool(output, "aimVisibleOnly", g_config.aimVisibleOnly);

            WriteBool(output, "triggerEnabled", g_config.triggerEnabled);
            WriteBind(output, "triggerBind", g_config.triggerBind);
            WriteBool(output, "triggerVisibleOnly", g_config.triggerVisibleOnly);
            WriteBool(output, "triggerAimAssist", g_config.triggerAimAssist);
            WriteInt(output, "triggerTargetBone", g_config.triggerTargetBone);
            WriteFloat(output, "triggerTapTimeMs", g_config.triggerTapTimeMs);
            WriteFloat(output, "triggerCooldownMs", g_config.triggerCooldownMs);

            WriteBool(output, "espEnemyBox", g_config.espEnemyBox);
            WriteBool(output, "espEnemyName", g_config.espEnemyName);
            WriteBool(output, "espEnemyHealth", g_config.espEnemyHealth);
            WriteBool(output, "espEnemyHpText", g_config.espEnemyHpText);
            WriteBool(output, "espEnemyTracer", g_config.espEnemyTracer);
            WriteBool(output, "espEnemySkeleton", g_config.espEnemySkeleton);
            WriteBool(output, "espEnemyWeaponText", g_config.espEnemyWeaponText);
            WriteBool(output, "espEnemyWeaponIcon", g_config.espEnemyWeaponIcon);
            WriteColor(output, "espEnemyColor", g_config.espEnemyColor);
            WriteColor(output, "espEnemyTracerColor", g_config.espEnemyTracerColor);
            WriteColor(output, "espEnemySkeletonColor", g_config.espEnemySkeletonColor);
            WriteColor(output, "espEnemyNameColor", g_config.espEnemyNameColor);
            WriteColor(output, "espEnemyHealthColor", g_config.espEnemyHealthColor);
            WriteColor(output, "espEnemyWeaponColor", g_config.espEnemyWeaponColor);

            WriteBool(output, "espTeamBox", g_config.espTeamBox);
            WriteBool(output, "espTeamName", g_config.espTeamName);
            WriteBool(output, "espTeamHealth", g_config.espTeamHealth);
            WriteBool(output, "espTeamHpText", g_config.espTeamHpText);
            WriteBool(output, "espTeamTracer", g_config.espTeamTracer);
            WriteBool(output, "espTeamSkeleton", g_config.espTeamSkeleton);
            WriteBool(output, "espTeamWeaponText", g_config.espTeamWeaponText);
            WriteBool(output, "espTeamWeaponIcon", g_config.espTeamWeaponIcon);
            WriteColor(output, "espTeamColor", g_config.espTeamColor);
            WriteColor(output, "espTeamTracerColor", g_config.espTeamTracerColor);
            WriteColor(output, "espTeamSkeletonColor", g_config.espTeamSkeletonColor);
            WriteColor(output, "espTeamNameColor", g_config.espTeamNameColor);
            WriteColor(output, "espTeamHealthColor", g_config.espTeamHealthColor);
            WriteColor(output, "espTeamWeaponColor", g_config.espTeamWeaponColor);

            WriteInt(output, "espBoxType", g_config.espBoxType);
            WriteFloat(output, "espBoxThickness", g_config.espBoxThickness);
            WriteBool(output, "espBoxOutline", g_config.espBoxOutline);
            WriteFloat(output, "espCornerBoxThickness", g_config.espCornerBoxThickness);
            WriteBool(output, "espCornerBoxOutline", g_config.espCornerBoxOutline);
            WriteFloat(output, "espSkeletonThickness", g_config.espSkeletonThickness);
            WriteBool(output, "espSkeletonOutline", g_config.espSkeletonOutline);
            WriteFloat(output, "espTracerThickness", g_config.espTracerThickness);
            WriteBool(output, "espTracerOutline", g_config.espTracerOutline);
            WriteFloat(output, "espTextSize", g_config.espTextSize);
            WriteBool(output, "espTextOutline", g_config.espTextOutline);
            WriteBool(output, "espShowDistance", g_config.espShowDistance);
            WriteBool(output, "espShowPlatform", g_config.espShowPlatform);
            WriteBool(output, "espShowRank", g_config.espShowRank);
            WriteBool(output, "espShowAmmo", g_config.espShowAmmo);
            WriteBool(output, "espFovTriangles", g_config.espFovTriangles);
            WriteBool(output, "espFovTriangleFilled", g_config.espFovTriangleFilled);
            WriteBool(output, "espFovTriangleOutline", g_config.espFovTriangleOutline);
            WriteBool(output, "espFovTriangleDynamic", g_config.espFovTriangleDynamic);
            WriteFloat(output, "espFovTriangleSize", g_config.espFovTriangleSize);
            WriteFloat(output, "espFovTriangleOffset", g_config.espFovTriangleOffset);
            WriteFloat(output, "espFovTriangleWidth", g_config.espFovTriangleWidth);

            WriteBool(output, "lootChests", g_config.lootChests);
            WriteColor(output, "lootChestColor", g_config.lootChestColor);
            WriteBool(output, "lootPickups", g_config.lootPickups);
            WriteBool(output, "lootDrops", g_config.lootDrops);
            WriteFloat(output, "lootMaxDist", g_config.lootMaxDist);
            WriteBool(output, "lootShowDist", g_config.lootShowDist);

            WriteBool(output, "watermarkEnabled", g_config.watermarkEnabled);
            WriteInt(output, "menuKey", g_config.menuKey);
            WriteInt(output, "exitKey", g_config.exitKey);

            WriteBool(output, "rageWalls", g_config.rageWalls);
            WriteBool(output, "rageSpeed", g_config.rageSpeed);
            WriteFloat(output, "rageSpeedVal", g_config.rageSpeedVal);
            WriteBool(output, "rageInfAmmo", g_config.rageInfAmmo);
            WriteBool(output, "rageNoReload", g_config.rageNoReload);
            WriteBool(output, "rageRapidFire", g_config.rageRapidFire);
            WriteBool(output, "rageFovOn", g_config.rageFovOn);
            WriteFloat(output, "rageFovVal", g_config.rageFovVal);
            WriteBool(output, "rageSpin", g_config.rageSpin);
            WriteInt(output, "rageZoomKey", g_config.rageZoomKey);
            WriteFloat(output, "rageZoomFov", g_config.rageZoomFov);
            WriteInt(output, "ragePanicKey", g_config.ragePanicKey);
        }

        bool SaveConfigToDisk(const std::string& configName) {
            const std::filesystem::path configPath = GetConfigPath(configName);
            std::ofstream output(configPath, std::ios::trunc);
            if (!output.is_open())
                return false;

            output.imbue(std::locale::classic());
            output << std::fixed << std::setprecision(4);
            SaveAllConfig(output);
            return output.good();
        }

        void LoadAllConfig(const std::unordered_map<std::string, std::string>& values, Config& loaded) {
            ParseBool(values, "aimEnabled", loaded.aimEnabled);
            ParseFloat(values, "aimFov", loaded.aimFov);
            ParseFloat(values, "aimSmooth", loaded.aimSmooth);
            ParseInt(values, "aimTargetBone", loaded.aimTargetBone);
            ParseBind(values, "aimBind", loaded.aimBind);
            ParseBool(values, "aimShowFov", loaded.aimShowFov);
            ParseColor(values, "aimFovColor", loaded.aimFovColor);
            ParseBool(values, "aimVisibleOnly", loaded.aimVisibleOnly);

            ParseBool(values, "triggerEnabled", loaded.triggerEnabled);
            ParseBind(values, "triggerBind", loaded.triggerBind);
            ParseBool(values, "triggerVisibleOnly", loaded.triggerVisibleOnly);
            ParseBool(values, "triggerAimAssist", loaded.triggerAimAssist);
            ParseInt(values, "triggerTargetBone", loaded.triggerTargetBone);
            ParseFloat(values, "triggerTapTimeMs", loaded.triggerTapTimeMs);
            ParseFloat(values, "triggerCooldownMs", loaded.triggerCooldownMs);

            ParseBool(values, "espEnemyBox", loaded.espEnemyBox);
            ParseBool(values, "espEnemyName", loaded.espEnemyName);
            ParseBool(values, "espEnemyHealth", loaded.espEnemyHealth);
            ParseBool(values, "espEnemyHpText", loaded.espEnemyHpText);
            ParseBool(values, "espEnemyTracer", loaded.espEnemyTracer);
            ParseBool(values, "espEnemySkeleton", loaded.espEnemySkeleton);
            ParseBool(values, "espEnemyWeaponText", loaded.espEnemyWeaponText);
            ParseBool(values, "espEnemyWeaponIcon", loaded.espEnemyWeaponIcon);
            ParseColor(values, "espEnemyColor", loaded.espEnemyColor);
            ParseColor(values, "espEnemyTracerColor", loaded.espEnemyTracerColor);
            ParseColor(values, "espEnemySkeletonColor", loaded.espEnemySkeletonColor);
            ParseColor(values, "espEnemyNameColor", loaded.espEnemyNameColor);
            ParseColor(values, "espEnemyHealthColor", loaded.espEnemyHealthColor);
            ParseColor(values, "espEnemyWeaponColor", loaded.espEnemyWeaponColor);

            ParseBool(values, "espTeamBox", loaded.espTeamBox);
            ParseBool(values, "espTeamName", loaded.espTeamName);
            ParseBool(values, "espTeamHealth", loaded.espTeamHealth);
            ParseBool(values, "espTeamHpText", loaded.espTeamHpText);
            ParseBool(values, "espTeamTracer", loaded.espTeamTracer);
            ParseBool(values, "espTeamSkeleton", loaded.espTeamSkeleton);
            ParseBool(values, "espTeamWeaponText", loaded.espTeamWeaponText);
            ParseBool(values, "espTeamWeaponIcon", loaded.espTeamWeaponIcon);
            ParseColor(values, "espTeamColor", loaded.espTeamColor);
            ParseColor(values, "espTeamTracerColor", loaded.espTeamTracerColor);
            ParseColor(values, "espTeamSkeletonColor", loaded.espTeamSkeletonColor);
            ParseColor(values, "espTeamNameColor", loaded.espTeamNameColor);
            ParseColor(values, "espTeamHealthColor", loaded.espTeamHealthColor);
            ParseColor(values, "espTeamWeaponColor", loaded.espTeamWeaponColor);

            ParseInt(values, "espBoxType", loaded.espBoxType);
            ParseFloat(values, "espBoxThickness", loaded.espBoxThickness);
            ParseBool(values, "espBoxOutline", loaded.espBoxOutline);
            ParseFloat(values, "espCornerBoxThickness", loaded.espCornerBoxThickness);
            ParseBool(values, "espCornerBoxOutline", loaded.espCornerBoxOutline);
            ParseFloat(values, "espSkeletonThickness", loaded.espSkeletonThickness);
            ParseBool(values, "espSkeletonOutline", loaded.espSkeletonOutline);
            ParseFloat(values, "espTracerThickness", loaded.espTracerThickness);
            ParseBool(values, "espTracerOutline", loaded.espTracerOutline);
            ParseFloat(values, "espTextSize", loaded.espTextSize);
            ParseBool(values, "espTextOutline", loaded.espTextOutline);
            ParseBool(values, "espShowDistance", loaded.espShowDistance);
            ParseBool(values, "espShowPlatform", loaded.espShowPlatform);
            ParseBool(values, "espShowRank", loaded.espShowRank);
            ParseBool(values, "espShowAmmo", loaded.espShowAmmo);
            ParseBool(values, "espFovTriangles", loaded.espFovTriangles);
            ParseBool(values, "espFovTriangleFilled", loaded.espFovTriangleFilled);
            ParseBool(values, "espFovTriangleOutline", loaded.espFovTriangleOutline);
            ParseBool(values, "espFovTriangleDynamic", loaded.espFovTriangleDynamic);
            ParseFloat(values, "espFovTriangleSize", loaded.espFovTriangleSize);
            ParseFloat(values, "espFovTriangleOffset", loaded.espFovTriangleOffset);
            ParseFloat(values, "espFovTriangleWidth", loaded.espFovTriangleWidth);

            ParseBool(values, "lootChests", loaded.lootChests);
            ParseColor(values, "lootChestColor", loaded.lootChestColor);
            ParseBool(values, "lootPickups", loaded.lootPickups);
            ParseBool(values, "lootDrops", loaded.lootDrops);
            ParseFloat(values, "lootMaxDist", loaded.lootMaxDist);
            ParseBool(values, "lootShowDist", loaded.lootShowDist);

            ParseBool(values, "watermarkEnabled", loaded.watermarkEnabled);
            ParseInt(values, "menuKey", loaded.menuKey);
            ParseInt(values, "exitKey", loaded.exitKey);

            ParseBool(values, "rageWalls", loaded.rageWalls);
            ParseBool(values, "rageSpeed", loaded.rageSpeed);
            ParseFloat(values, "rageSpeedVal", loaded.rageSpeedVal);
            ParseBool(values, "rageInfAmmo", loaded.rageInfAmmo);
            ParseBool(values, "rageNoReload", loaded.rageNoReload);
            ParseBool(values, "rageRapidFire", loaded.rageRapidFire);
            ParseBool(values, "rageFovOn", loaded.rageFovOn);
            ParseFloat(values, "rageFovVal", loaded.rageFovVal);
            ParseBool(values, "rageSpin", loaded.rageSpin);
            ParseInt(values, "rageZoomKey", loaded.rageZoomKey);
            ParseFloat(values, "rageZoomFov", loaded.rageZoomFov);
            ParseInt(values, "ragePanicKey", loaded.ragePanicKey);

            loaded.aimBind.mode = std::clamp(loaded.aimBind.mode, static_cast<int>(BindMode_AlwaysOn), static_cast<int>(BindMode_Hold));
            loaded.triggerBind.mode = std::clamp(loaded.triggerBind.mode, static_cast<int>(BindMode_AlwaysOn), static_cast<int>(BindMode_Hold));
            loaded.aimTargetBone = std::clamp(loaded.aimTargetBone, 0, 1);
            loaded.triggerTargetBone = std::clamp(loaded.triggerTargetBone, 0, 1);
            loaded.espBoxType = std::clamp(loaded.espBoxType, 0, 1);
            if (loaded.menuKey == 0) loaded.menuKey = VK_INSERT;
            if (loaded.exitKey == 0) loaded.exitKey = VK_ESCAPE;
        }

        bool LoadConfigFromDisk(const std::string& configName) {
            std::ifstream input(GetConfigPath(configName));
            if (!input.is_open())
                return false;

            std::unordered_map<std::string, std::string> values;
            std::string line;
            while (std::getline(input, line)) {
                if (line.empty() || line[0] == '#')
                    continue;

                const size_t separator = line.find('=');
                if (separator == std::string::npos)
                    continue;

                values[Trim(line.substr(0, separator))] = Trim(line.substr(separator + 1));
            }

            Config loaded{};
            LoadAllConfig(values, loaded);
            g_config = loaded;
            return true;
        }

        bool DeleteConfigFromDisk(const std::string& configName) {
            return std::filesystem::remove(GetConfigPath(configName));
        }

    } // namespace

    void ReloadAutosave() {
        if (std::filesystem::exists(GetConfigPath(AutosaveName())))
            LoadConfigFromDisk(AutosaveName());
        sync();
    }

    void SaveAutosave() {
        if (!SaveConfigToDisk(AutosaveName()))
            return;
        RefreshConfigList();
    }

    // ---- sync: menu config -> live feature namespaces ---------------------

    void sync() {
        using namespace bridge;

        aimbot::enabled = g_config.aimEnabled;
        aimbot::show_fov = g_config.aimShowFov;
        aimbot::fov_color = ImVec4(
            g_config.aimFovColor[0],
            g_config.aimFovColor[1],
            g_config.aimFovColor[2],
            g_config.aimFovColor[3]);
        aimbot::fov = static_cast<int>(g_config.aimFov);
        aimbot::smoothness = static_cast<int>(g_config.aimSmooth);
        aimbot::aimbot_keybind = g_config.aimBind.key;
        aimbot::aim_mode = g_config.aimBind.mode;
        aimbot::target_bone = g_config.aimTargetBone;
        aimbot::visible_only = g_config.aimVisibleOnly;

        triggerbot::enabled = g_config.triggerEnabled;
        triggerbot::bind = g_config.triggerBind.key;
        triggerbot::bind_mode = g_config.triggerBind.mode;
        triggerbot::visible_only = g_config.triggerVisibleOnly;
        triggerbot::aim_assist = g_config.triggerAimAssist;
        triggerbot::target_bone = g_config.triggerTargetBone;
        triggerbot::tap_time_ms = g_config.triggerTapTimeMs;
        triggerbot::cooldown_ms = g_config.triggerCooldownMs;
        triggerbot::show_fov = g_config.aimShowFov;

        rage::walls = g_config.rageWalls;
        rage::speed = g_config.rageSpeed;
        rage::speedVal = g_config.rageSpeedVal;
        rage::infAmmo = g_config.rageInfAmmo;
        rage::noReload = g_config.rageNoReload;
        rage::rapidFire = g_config.rageRapidFire;
        rage::fovOn = g_config.rageFovOn;
        rage::fovVal = g_config.rageFovVal;
        rage::spinbot = g_config.rageSpin;
        rage::zoomKey = g_config.rageZoomKey;
        rage::zoomFov = g_config.rageZoomFov;
        rage::panicKey = g_config.ragePanicKey;

        esp::enabled = g_config.espEnemyBox || g_config.espEnemyName || g_config.espEnemyHealth ||
            g_config.espEnemyHpText || g_config.espEnemyTracer || g_config.espEnemySkeleton ||
            g_config.espEnemyWeaponText || g_config.espEnemyWeaponIcon;
        esp::team_enabled = g_config.espTeamBox || g_config.espTeamName || g_config.espTeamHealth ||
            g_config.espTeamHpText || g_config.espTeamTracer || g_config.espTeamSkeleton ||
            g_config.espTeamWeaponText || g_config.espTeamWeaponIcon;
        esp::box = g_config.espEnemyBox;
        esp::skeleton = g_config.espEnemySkeleton;
        esp::tracers = g_config.espEnemyTracer;
        esp::show_name = g_config.espEnemyName;
        esp::show_health = g_config.espEnemyHealth;
        esp::show_hp_text = g_config.espEnemyHpText;
        esp::show_weapon = g_config.espEnemyWeaponText;
        esp::show_weapon_icon = g_config.espEnemyWeaponIcon;
        esp::box_type = g_config.espBoxType;
        esp::box_thickness = g_config.espBoxThickness;
        esp::box_outline = g_config.espBoxOutline;
        esp::corner_box_thickness = g_config.espCornerBoxThickness;
        esp::corner_box_outline = g_config.espCornerBoxOutline;
        esp::skeleton_thickness = g_config.espSkeletonThickness;
        esp::skeleton_outline = g_config.espSkeletonOutline;
        esp::tracers_thickness = g_config.espTracerThickness;
        esp::tracers_outline = g_config.espTracerOutline;
        esp::text_size = g_config.espTextSize;
        esp::text_outline = g_config.espTextOutline;
        esp::show_distance = g_config.espShowDistance;
        esp::show_platform = g_config.espShowPlatform;
        esp::show_rank = g_config.espShowRank;
        esp::show_ammo = g_config.espShowAmmo;
        esp::fov_triangles = g_config.espFovTriangles;
        esp::fov_triangle_filled = g_config.espFovTriangleFilled;
        esp::fov_triangle_outline = g_config.espFovTriangleOutline;
        esp::fov_triangle_dynamic = g_config.espFovTriangleDynamic;
        esp::fov_triangle_size = g_config.espFovTriangleSize;
        esp::fov_triangle_offset = g_config.espFovTriangleOffset;
        esp::fov_triangle_width = g_config.espFovTriangleWidth;

        loot::wantChests = g_config.lootChests;
        loot::wantPickups = g_config.lootPickups;
        loot::wantDrops = g_config.lootDrops;
        loot::showDist = g_config.lootShowDist;
        loot::maxDist = g_config.lootMaxDist;
        loot::chestColor = ImGui::ColorConvertFloat4ToU32(ImVec4(
            g_config.lootChestColor[0], g_config.lootChestColor[1],
            g_config.lootChestColor[2], g_config.lootChestColor[3]));

        esp::team_box = g_config.espTeamBox;
        esp::team_name = g_config.espTeamName;
        esp::team_health = g_config.espTeamHealth;
        esp::team_hp_text = g_config.espTeamHpText;
        esp::team_tracer = g_config.espTeamTracer;
        esp::team_skeleton = g_config.espTeamSkeleton;
        esp::team_weapon_text = g_config.espTeamWeaponText;
        esp::team_weapon_icon = g_config.espTeamWeaponIcon;

        esp::enemy_color = ImGui::ColorConvertFloat4ToU32(ImVec4(
            g_config.espEnemyColor[0], g_config.espEnemyColor[1], g_config.espEnemyColor[2], g_config.espEnemyColor[3]));
        esp::enemy_tracer_color = ImGui::ColorConvertFloat4ToU32(ImVec4(
            g_config.espEnemyTracerColor[0], g_config.espEnemyTracerColor[1], g_config.espEnemyTracerColor[2], g_config.espEnemyTracerColor[3]));
        esp::enemy_skeleton_color = ImGui::ColorConvertFloat4ToU32(ImVec4(
            g_config.espEnemySkeletonColor[0], g_config.espEnemySkeletonColor[1], g_config.espEnemySkeletonColor[2], g_config.espEnemySkeletonColor[3]));
        esp::enemy_name_color = ImGui::ColorConvertFloat4ToU32(ImVec4(
            g_config.espEnemyNameColor[0], g_config.espEnemyNameColor[1], g_config.espEnemyNameColor[2], g_config.espEnemyNameColor[3]));
        esp::enemy_health_color = ImGui::ColorConvertFloat4ToU32(ImVec4(
            g_config.espEnemyHealthColor[0], g_config.espEnemyHealthColor[1], g_config.espEnemyHealthColor[2], g_config.espEnemyHealthColor[3]));

        esp::team_color = ImGui::ColorConvertFloat4ToU32(ImVec4(
            g_config.espTeamColor[0], g_config.espTeamColor[1], g_config.espTeamColor[2], g_config.espTeamColor[3]));
        esp::team_tracer_color = ImGui::ColorConvertFloat4ToU32(ImVec4(
            g_config.espTeamTracerColor[0], g_config.espTeamTracerColor[1], g_config.espTeamTracerColor[2], g_config.espTeamTracerColor[3]));
        esp::team_skeleton_color = ImGui::ColorConvertFloat4ToU32(ImVec4(
            g_config.espTeamSkeletonColor[0], g_config.espTeamSkeletonColor[1], g_config.espTeamSkeletonColor[2], g_config.espTeamSkeletonColor[3]));
        esp::team_name_color = ImGui::ColorConvertFloat4ToU32(ImVec4(
            g_config.espTeamNameColor[0], g_config.espTeamNameColor[1], g_config.espTeamNameColor[2], g_config.espTeamNameColor[3]));
        esp::team_health_color = ImGui::ColorConvertFloat4ToU32(ImVec4(
            g_config.espTeamHealthColor[0], g_config.espTeamHealthColor[1], g_config.espTeamHealthColor[2], g_config.espTeamHealthColor[3]));

        esp::watermark_enabled = g_config.watermarkEnabled;
        io_state::menu_key = GetMenuKey();
        io_state::exit_key = GetExitKey();
    }

    // ---- key binding helper ------------------------------------------------

    namespace {

        const char* KeyName(int vk) {
            switch (vk) {
            case 0: return "None";
            case VK_LBUTTON: return "LMB";
            case VK_RBUTTON: return "RMB";
            case VK_MBUTTON: return "MMB";
            case VK_XBUTTON1: return "Mouse4";
            case VK_XBUTTON2: return "Mouse5";
            case VK_BACK: return "Backspace";
            case VK_TAB: return "Tab";
            case VK_RETURN: return "Enter";
            case VK_SHIFT: return "Shift";
            case VK_CONTROL: return "Ctrl";
            case VK_MENU: return "Alt";
            case VK_CAPITAL: return "Caps Lock";
            case VK_ESCAPE: return "Esc";
            case VK_SPACE: return "Space";
            case VK_PRIOR: return "Page Up";
            case VK_NEXT: return "Page Down";
            case VK_END: return "End";
            case VK_HOME: return "Home";
            case VK_LEFT: return "Left";
            case VK_UP: return "Up";
            case VK_RIGHT: return "Right";
            case VK_DOWN: return "Down";
            case VK_INSERT: return "Insert";
            case VK_DELETE: return "Delete";
            case VK_F1: case VK_F2: case VK_F3: case VK_F4: case VK_F5: case VK_F6:
            case VK_F7: case VK_F8: case VK_F9: case VK_F10: case VK_F11: case VK_F12:
            {
                static char buf[16];
                buf[0] = 'F';
                const int n = vk - VK_F1 + 1;
                if (n < 10) buf[1] = (char)('0' + n); else { buf[1] = '1'; buf[2] = (char)('0' + (n - 10)); }
                buf[n >= 10 ? 3 : 2] = '\0';
                return buf;
            }
            default:
            {
                static char buf[8];
                if (vk >= '0' && vk <= '9') { buf[0] = (char)vk; buf[1] = '\0'; return buf; }
                if (vk >= 'A' && vk <= 'Z') { buf[0] = (char)vk; buf[1] = '\0'; return buf; }
                return "Key";
            }
            }
        }

        bool g_capturingKey = false;
        int* g_captureTarget = nullptr;
        BYTE g_prevKeyState[256]{};

        void DrawKeyPicker(const char* id, int* key) {
            const bool isCapturing = g_capturingKey && g_captureTarget == key;

            std::string label = "Key##";
            label += id;
            if (ImGui::Button(isCapturing ? "[ Press key... ]" : KeyName(*key), ImVec2(120, 0))) {
                g_capturingKey = true;
                g_captureTarget = key;
                GetKeyboardState(g_prevKeyState);
            }

            if (isCapturing) {
                if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
                    g_capturingKey = false;
                    g_captureTarget = nullptr;
                    return;
                }

                BYTE cur[256]{};
                GetKeyboardState(cur);
                for (int vk = 0x01; vk < 0x100; ++vk) {
                    if (vk == VK_ESCAPE || vk == VK_LWIN || vk == VK_RWIN)
                        continue;
                    const bool wasDown = (g_prevKeyState[vk] & 0x80) != 0;
                    const bool isDown = (cur[vk] & 0x80) != 0;
                    if (isDown && !wasDown) {
                        *key = vk;
                        g_capturingKey = false;
                        g_captureTarget = nullptr;
                        return;
                    }
                }
            }
        }

        void DrawBindEditor(const char* idPrefix, KeyBind& bind) {
            std::string modeLabel = "Activation##";
            modeLabel += idPrefix;
            ImGui::Combo(modeLabel.c_str(), &bind.mode, kBindModeItems, IM_ARRAYSIZE(kBindModeItems));

            if (bind.mode == BindMode_Hold)
                DrawKeyPicker(idPrefix, &bind.key);
        }

        void DrawColorSetting(const char* label, const char* id, float color[4]) {
            ImGui::TextUnformatted(label);
            ImGui::SameLine(180.0f);
            ImGui::SetNextItemWidth(210.0f);
            ImGui::ColorEdit4(id, color, ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_AlphaBar);
        }

    } // namespace

    // ---- config tab ui ----------------------------------------------------

    namespace {

        void DrawConfigSection() {
            auto& ui = ConfigUi();
            ImGui::TextColored(ImVec4(0.88f, 0.75f, 0.42f, 1.0f), "Configs");
            ImGui::Separator();

            const char* preview = (ui.selectedIndex >= 0 && ui.selectedIndex < static_cast<int>(ui.configNames.size()))
                ? ui.configNames[ui.selectedIndex].c_str()
                : "No configs";

            if (ImGui::BeginCombo("Selected Config", preview)) {
                for (int i = 0; i < static_cast<int>(ui.configNames.size()); ++i) {
                    const bool isSelected = ui.selectedIndex == i;
                    if (ImGui::Selectable(ui.configNames[i].c_str(), isSelected))
                        ui.selectedIndex = i;
                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::InputText("New Config Name", ui.newConfigName, IM_ARRAYSIZE(ui.newConfigName));

            if (ImGui::Button("Create", ImVec2(92.0f, 0.0f))) {
                const std::string configName = SanitizeConfigName(ui.newConfigName);
                if (configName.empty()) {
                    ui.status = "Enter a valid config name.";
                } else if (std::filesystem::exists(GetConfigPath(configName))) {
                    ui.status = "Config already exists. Use Save to overwrite it.";
                } else if (SaveConfigToDisk(configName)) {
                    RefreshConfigList(configName);
                    ui.status = "Config created.";
                } else {
                    ui.status = "Failed to create config.";
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("Refresh", ImVec2(92.0f, 0.0f))) {
                RefreshConfigList();
                ui.status = "Config list refreshed.";
            }

            const bool hasSelection = ui.selectedIndex >= 0 && ui.selectedIndex < static_cast<int>(ui.configNames.size());

            ImGui::BeginDisabled(!hasSelection);
            if (ImGui::Button("Save", ImVec2(92.0f, 0.0f)) && hasSelection) {
                if (SaveConfigToDisk(ui.configNames[ui.selectedIndex])) {
                    ui.status = "Config saved.";
                } else {
                    ui.status = "Failed to save config.";
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("Load", ImVec2(92.0f, 0.0f)) && hasSelection) {
                if (LoadConfigFromDisk(ui.configNames[ui.selectedIndex])) {
                    sync();
                    ui.status = "Config loaded.";
                } else {
                    ui.status = "Failed to load config.";
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("Delete", ImVec2(92.0f, 0.0f)) && hasSelection) {
                const std::string configName = ui.configNames[ui.selectedIndex];
                if (DeleteConfigFromDisk(configName)) {
                    RefreshConfigList();
                    ui.status = "Config deleted.";
                } else {
                    ui.status = "Failed to delete config.";
                }
            }
            ImGui::EndDisabled();

            const std::string cfgPathText = GetConfigPath("x").parent_path().string();
            ImGui::TextDisabled("Folder: %s", cfgPathText.c_str());
            if (!ui.status.empty())
                ImGui::TextDisabled("%s", ui.status.c_str());
        }

    } // namespace

    // ---- aim tab ui --------------------------------------------------------

    namespace {

        void DrawAimbotSettings() {
            ImGui::TextColored(ImVec4(0.88f, 0.75f, 0.42f, 1.0f), "Aimbot Engine");
            ImGui::Separator();
            ImGui::Checkbox("Enable Aimbot", &g_config.aimEnabled);

            if (g_config.aimEnabled) {
                DrawBindEditor("aim_bind", g_config.aimBind);
                ImGui::SliderFloat("Aimbot FOV", &g_config.aimFov, 25.0f, 300.0f, "%.0f");
                ImGui::SliderFloat("Smoothing", &g_config.aimSmooth, 1.0f, 20.0f, "%.1f");
                ImGui::Combo("Aim Point", &g_config.aimTargetBone, kAimPointItems, IM_ARRAYSIZE(kAimPointItems));
                ImGui::Checkbox("Visible targets only", &g_config.aimVisibleOnly);
                ImGui::Checkbox("Show FOV", &g_config.aimShowFov);
                if (g_config.aimShowFov)
                    DrawColorSetting("FOV Color", "##aim_fov_color", g_config.aimFovColor);
                ImGui::Spacing();
                ImGui::TextDisabled("Smoothing 1.0 is instant.");
            }
        }

        void DrawTriggerSettings() {
            ImGui::TextColored(ImVec4(0.88f, 0.75f, 0.42f, 1.0f), "Triggerbot Engine");
            ImGui::Separator();
            ImGui::Checkbox("Enable Triggerbot", &g_config.triggerEnabled);

            if (g_config.triggerEnabled) {
                DrawBindEditor("trigger_bind", g_config.triggerBind);
                ImGui::Checkbox("Visible targets only", &g_config.triggerVisibleOnly);
                ImGui::Checkbox("Snap before shot", &g_config.triggerAimAssist);
                ImGui::Combo("Aim Point", &g_config.triggerTargetBone, kAimPointItems, IM_ARRAYSIZE(kAimPointItems));
                ImGui::SliderFloat("Tap Time (ms)", &g_config.triggerTapTimeMs, 0.0f, 25.0f, "%.0f");
                ImGui::SliderFloat("Cooldown (ms)", &g_config.triggerCooldownMs, 10.0f, 250.0f, "%.0f");
                ImGui::Spacing();
                ImGui::TextDisabled("Simulates LMB while the crosshair is on a target.");
            }
        }

        void RenderAimTab() {
            static int aimSubTab = 0;

            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
            if (ImGui::Button("Aimbot", ImVec2(120, 34))) aimSubTab = 0;
            ImGui::SameLine();
            if (ImGui::Button("Trigger", ImVec2(120, 34))) aimSubTab = 1;
            ImGui::PopStyleVar();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::BeginChild("AimCard", ImVec2(0, 0), true);
            if (aimSubTab == 0)
                DrawAimbotSettings();
            else
                DrawTriggerSettings();
            ImGui::EndChild();
        }

    } // namespace

    // ---- esp tab ui --------------------------------------------------------

    namespace {

        void DrawEspCore(const EspSettingsRef& s, const char* idPrefix, const char* title) {
            const std::string boxId = std::string("##box_") + idPrefix;
            const std::string nameId = std::string("##name_") + idPrefix;
            const std::string healthId = std::string("##health_") + idPrefix;
            const std::string hpTextId = std::string("##hptext_") + idPrefix;
            const std::string tracerId = std::string("##tracer_") + idPrefix;
            const std::string skeletonId = std::string("##skeleton_") + idPrefix;
            const std::string weaponId = std::string("##weapon_") + idPrefix;
            const std::string weaponTextId = std::string("##weapontext_") + idPrefix;
            const std::string weaponIconId = std::string("##weaponicon_") + idPrefix;

            ImGui::TextColored(ImVec4(0.88f, 0.75f, 0.42f, 1.0f), title);
            ImGui::Separator();
            ImGui::Checkbox("Box", &s.box);
            ImGui::Checkbox("Name", &s.name);
            ImGui::Checkbox("Health Bar", &s.health);
            ImGui::Checkbox("HP Text", &s.hpText);
            ImGui::Checkbox("Tracer", &s.tracer);
            ImGui::Checkbox("Skeleton", &s.skeleton);
            ImGui::Checkbox("Weapon Text", &s.weaponText);
            ImGui::Checkbox("Weapon Icon", &s.weaponIcon);
            ImGui::Spacing();
            if (s.box)
                DrawColorSetting("Box Color", boxId.c_str(), s.boxColor);
            if (s.name)
                DrawColorSetting("Name Color", nameId.c_str(), s.nameColor);
            if (s.health || s.hpText)
                DrawColorSetting("Health Color", healthId.c_str(), s.healthColor);
            if (s.tracer)
                DrawColorSetting("Tracer Color", tracerId.c_str(), s.tracerColor);
            if (s.skeleton)
                DrawColorSetting("Skeleton Color", skeletonId.c_str(), s.skeletonColor);
            if (s.weaponText || s.weaponIcon)
                DrawColorSetting("Weapon Color", weaponId.c_str(), s.weaponColor);
        }

        void DrawEspAdvanced() {
            bool open = ImGui::CollapsingHeader("Advanced", ImGuiTreeNodeFlags_DefaultOpen);
            if (!open) return;

            ImGui::Indent(6.0f);
            ImGui::Combo("Box Type", &g_config.espBoxType, kBoxTypeItems, IM_ARRAYSIZE(kBoxTypeItems));
            if (g_config.espBoxType != 1) {
                ImGui::PushID("box");
                ImGui::SliderFloat("Box Thickness", &g_config.espBoxThickness, 1.0f, 5.0f, "%.1f px");
                ImGui::Checkbox("Box Outline", &g_config.espBoxOutline);
                ImGui::PopID();
            }
            if (g_config.espBoxType != 0) {
                ImGui::PushID("corner_box");
                ImGui::SliderFloat("Corner Thickness", &g_config.espCornerBoxThickness, 1.0f, 5.0f, "%.1f px");
                ImGui::Checkbox("Corner Outline", &g_config.espCornerBoxOutline);
                ImGui::PopID();
            }
            ImGui::SliderFloat("Skeleton Thickness", &g_config.espSkeletonThickness, 1.0f, 5.0f, "%.1f px");
            ImGui::Checkbox("Skeleton Outline", &g_config.espSkeletonOutline);
            ImGui::SliderFloat("Tracer Thickness", &g_config.espTracerThickness, 1.0f, 5.0f, "%.1f px");
            ImGui::Checkbox("Tracer Outline", &g_config.espTracerOutline);
            ImGui::SliderFloat("Flag Text Size", &g_config.espTextSize, 10.0f, 30.0f, "%.0f px");
            ImGui::Checkbox("Flag Text Outline", &g_config.espTextOutline);
            ImGui::Separator();
            ImGui::Checkbox("Show Distance", &g_config.espShowDistance);
            ImGui::Checkbox("Show Platform", &g_config.espShowPlatform);
            ImGui::Checkbox("Show Rank", &g_config.espShowRank);
            ImGui::Checkbox("Show Ammo", &g_config.espShowAmmo);
            ImGui::Separator();
            ImGui::Checkbox("FOV Arrows", &g_config.espFovTriangles);
            if (g_config.espFovTriangles) {
                ImGui::Indent(6.0f);
                ImGui::Checkbox("Filled", &g_config.espFovTriangleFilled);
                ImGui::SameLine();
                ImGui::Checkbox("Outline", &g_config.espFovTriangleOutline);
                ImGui::Checkbox("Dynamic scaling", &g_config.espFovTriangleDynamic);
                ImGui::SliderFloat("Size", &g_config.espFovTriangleSize, 2.0f, 30.0f, "%.0f px");
                ImGui::SliderFloat("Spacing from circle", &g_config.espFovTriangleOffset, 0.0f, 80.0f, "%.0f px");
                ImGui::SliderFloat("Width", &g_config.espFovTriangleWidth, 1.7f, 2.9f, "%.2f");
                ImGui::Unindent(6.0f);
            }
            ImGui::Unindent(6.0f);
        }

        void RenderEspTab() {
            static int espSubTab = 0;

            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
            if (ImGui::Button("Enemy", ImVec2(170, 34))) espSubTab = 0;
            ImGui::SameLine();
            if (ImGui::Button("Team", ImVec2(170, 34))) espSubTab = 1;
            ImGui::SameLine();
            if (ImGui::Button("Advanced", ImVec2(170, 34))) espSubTab = 2;
            ImGui::PopStyleVar();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::BeginChild("EspCard", ImVec2(0, 0), true);
            EspSettingsRef enemy{
                g_config.espEnemyBox,
                g_config.espEnemyName,
                g_config.espEnemyHealth,
                g_config.espEnemyHpText,
                g_config.espEnemyTracer,
                g_config.espEnemySkeleton,
                g_config.espEnemyWeaponText,
                g_config.espEnemyWeaponIcon,
                g_config.espEnemyColor,
                g_config.espEnemyNameColor,
                g_config.espEnemyHealthColor,
                g_config.espEnemyTracerColor,
                g_config.espEnemySkeletonColor,
                g_config.espEnemyWeaponColor
            };
            EspSettingsRef team{
                g_config.espTeamBox,
                g_config.espTeamName,
                g_config.espTeamHealth,
                g_config.espTeamHpText,
                g_config.espTeamTracer,
                g_config.espTeamSkeleton,
                g_config.espTeamWeaponText,
                g_config.espTeamWeaponIcon,
                g_config.espTeamColor,
                g_config.espTeamNameColor,
                g_config.espTeamHealthColor,
                g_config.espTeamTracerColor,
                g_config.espTeamSkeletonColor,
                g_config.espTeamWeaponColor
            };

            if (espSubTab == 0)
                DrawEspCore(enemy, "enemy", "Enemy ESP");
            else if (espSubTab == 1)
                DrawEspCore(team, "team", "Team ESP");
            else
                DrawEspAdvanced();
            ImGui::EndChild();
        }

    } // namespace

    // ---- misc tab ui --------------------------------------------------------

    namespace {

        void DrawMainSettings() {
            ImGui::TextColored(ImVec4(0.88f, 0.75f, 0.42f, 1.0f), "Main");
            ImGui::Separator();
            ImGui::Checkbox("Watermark", &g_config.watermarkEnabled);
            ImGui::Checkbox("Show Distance", &g_config.espShowDistance);
            ImGui::Checkbox("Show Platform", &g_config.espShowPlatform);
            ImGui::Checkbox("Show Rank", &g_config.espShowRank);
            ImGui::Checkbox("Show Ammo", &g_config.espShowAmmo);
            ImGui::Separator();
            ImGui::Checkbox("FOV Arrows", &g_config.espFovTriangles);
            if (g_config.espFovTriangles) {
                ImGui::SliderFloat("Size", &g_config.espFovTriangleSize, 2.0f, 30.0f, "%.0f px");
                ImGui::SliderFloat("Spacing from circle", &g_config.espFovTriangleOffset, 0.0f, 80.0f, "%.0f px");
            }
        }

        void DrawSettingsTab() {
            ImGui::TextColored(ImVec4(0.88f, 0.75f, 0.42f, 1.0f), "Settings");
            ImGui::Separator();
            ImGui::TextUnformatted("Menu Hotkey");
            ImGui::SameLine(140.0f);
            DrawKeyPicker("menu_key", &g_config.menuKey);
            ImGui::TextUnformatted("Exit Hotkey");
            ImGui::SameLine(140.0f);
            DrawKeyPicker("exit_key", &g_config.exitKey);
            ImGui::Spacing();
            ImGui::TextDisabled("Presets are saved to cfg\\*.cfg next to the executable.");
            ImGui::TextDisabled("Autosave writes every second and on exit.");
            ImGui::Spacing();
            ImGui::TextDisabled("Overlay title: Discord Overlay window.");
            ImGui::TextDisabled("Game: Fortnite (Windowed Fullscreen).");
            ImGui::TextColored(ImVec4(0.35f, 0.65f, 1.0f, 1.0f), "https://azurecheats.cc");
            ImGui::TextColored(ImVec4(0.88f, 0.75f, 0.42f, 1.0f), "https://discord.gg/azuresoftware");
        }

        void RenderMiscTab() {
            static int miscSubTab = 0;

            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
            if (ImGui::Button("Main", ImVec2(120, 34))) miscSubTab = 0;
            ImGui::SameLine();
            if (ImGui::Button("Config", ImVec2(120, 34))) miscSubTab = 1;
            ImGui::SameLine();
            if (ImGui::Button("Settings", ImVec2(120, 34))) miscSubTab = 2;
            ImGui::PopStyleVar();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::BeginChild("MiscCard", ImVec2(0, 0), true);
            if (miscSubTab == 0) {
                DrawMainSettings();
            } else if (miscSubTab == 1) {
                DrawConfigSection();
            } else {
                DrawSettingsTab();
            }
            ImGui::EndChild();
        }

    } // namespace

    // ---- theme + font --------------------------------------------------------

    namespace {

        bool FontFileExists(const char* path) {
            if (!path || !path[0])
                return false;
            const DWORD attrs = GetFileAttributesA(path);
            return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
        }

        ImFont* LoadMenuFont(ImGuiIO& io) {
            static const char* kCandidates[] = {
                "C:\\Windows\\Fonts\\bahnschrift.ttf",
                "C:\\Windows\\Fonts\\segoeuib.ttf",
                "C:\\Windows\\Fonts\\arialbd.ttf"
            };

            ImFontConfig config;
            config.OversampleH = 3;
            config.OversampleV = 2;
            config.PixelSnapH = false;
            config.RasterizerMultiply = 1.1f;

            const ImWchar* glyphRanges = io.Fonts->GetGlyphRangesCyrillic();
            for (const char* path : kCandidates) {
                if (!FontFileExists(path))
                    continue;
                if (ImFont* font = io.Fonts->AddFontFromFileTTF(path, kMenuFontSize, &config, glyphRanges))
                    return font;
            }

            return io.Fonts->AddFontDefault();
        }

        void ApplyMenuTheme() {
            ImGuiStyle& style = ImGui::GetStyle();
            style.WindowRounding = 12.0f;
            style.FrameRounding = 7.0f;
            style.ScrollbarRounding = 7.0f;
            style.GrabRounding = 7.0f;
            style.TabRounding = 7.0f;
            style.WindowBorderSize = 1.0f;
            style.FrameBorderSize = 1.0f;
            style.PopupBorderSize = 1.0f;
            style.WindowPadding = ImVec2(14, 14);
            style.FramePadding = ImVec2(10, 6);
            style.ItemSpacing = ImVec2(10, 8);
            style.ItemInnerSpacing = ImVec2(8, 6);

            ImVec4* colors = style.Colors;
            colors[ImGuiCol_WindowBg] = ImVec4(0.055f, 0.058f, 0.066f, 0.96f);
            colors[ImGuiCol_Border] = ImVec4(0.56f, 0.46f, 0.24f, 0.50f);
            colors[ImGuiCol_FrameBg] = ImVec4(0.11f, 0.11f, 0.13f, 0.90f);
            colors[ImGuiCol_FrameBgHovered] = ImVec4(0.24f, 0.19f, 0.12f, 0.96f);
            colors[ImGuiCol_FrameBgActive] = ImVec4(0.36f, 0.27f, 0.13f, 1.0f);
            colors[ImGuiCol_TitleBg] = ImVec4(0.06f, 0.06f, 0.07f, 1.0f);
            colors[ImGuiCol_TitleBgActive] = ImVec4(0.14f, 0.11f, 0.08f, 1.0f);
            colors[ImGuiCol_CheckMark] = ImVec4(0.95f, 0.83f, 0.46f, 1.0f);
            colors[ImGuiCol_SliderGrab] = ImVec4(0.83f, 0.67f, 0.30f, 1.0f);
            colors[ImGuiCol_SliderGrabActive] = ImVec4(0.95f, 0.82f, 0.45f, 1.0f);
            colors[ImGuiCol_Button] = ImVec4(0.16f, 0.13f, 0.10f, 0.92f);
            colors[ImGuiCol_ButtonHovered] = ImVec4(0.36f, 0.28f, 0.14f, 0.98f);
            colors[ImGuiCol_ButtonActive] = ImVec4(0.52f, 0.40f, 0.17f, 1.0f);
            colors[ImGuiCol_Header] = ImVec4(0.22f, 0.18f, 0.11f, 0.88f);
            colors[ImGuiCol_HeaderHovered] = ImVec4(0.38f, 0.30f, 0.15f, 0.94f);
            colors[ImGuiCol_HeaderActive] = ImVec4(0.52f, 0.40f, 0.18f, 1.0f);
            colors[ImGuiCol_Separator] = ImVec4(0.45f, 0.36f, 0.18f, 0.65f);
            colors[ImGuiCol_Text] = ImVec4(0.94f, 0.94f, 0.96f, 1.0f);
            colors[ImGuiCol_TextDisabled] = ImVec4(0.62f, 0.62f, 0.65f, 1.0f);
        }

        bool DrawSidebarButton(const char* label, bool active) {
            const ImVec4 idle(0.12f, 0.12f, 0.14f, 0.82f);
            const ImVec4 hover(0.26f, 0.22f, 0.14f, 0.95f);
            const ImVec4 pressed(0.52f, 0.38f, 0.16f, 1.0f);
            const ImVec4 border(0.65f, 0.53f, 0.28f, active ? 0.95f : 0.42f);

            ImGui::PushStyleColor(ImGuiCol_Button, active ? hover : idle);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active ? pressed : hover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, pressed);
            ImGui::PushStyleColor(ImGuiCol_Border, border);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

            const bool clicked = ImGui::Button(label, ImVec2(-1.0f, 36.0f));

            ImGui::PopStyleVar();
            ImGui::PopStyleColor(4);
            return clicked;
        }

    } // namespace

    // ---- startup ------------------------------------------------------------

    void Initialize() {
        ImGuiIO& io = ImGui::GetIO();
        g_menuFont = LoadMenuFont(io);
        io.FontDefault = g_menuFont;
        ImGui_ImplDX11_InvalidateDeviceObjects();
        ImGui_ImplDX11_CreateDeviceObjects();

        if (std::filesystem::exists(GetConfigPath(AutosaveName())))
            LoadConfigFromDisk(AutosaveName());
        RefreshConfigList();
        ApplyMenuTheme();
        sync();

        Discord::StartWorker();
    }

    // ---- render -------------------------------------------------------------

    bool RenderLegacy() {
        static int mainTab = 0;
        static float menuAlpha = 0.0f;
        menuAlpha = (std::min)(menuAlpha + ImGui::GetIO().DeltaTime * 3.2f, 1.0f);

        Menu::sync();

        // Menu key toggles the window; exit key tears the overlay down.
        static bool menuKeyWasDown = false;
        const bool menuKeyDown = (GetAsyncKeyState(io_state::menu_key) & 0x8000) != 0;
        if (menuKeyDown && !menuKeyWasDown)
            io_state::menu_open = !io_state::menu_open;
        menuKeyWasDown = menuKeyDown;

        if (io_state::menu_open) {
            static bool exitKeyWasDown = false;
            const bool exitKeyDown = (GetAsyncKeyState(io_state::exit_key) & 0x8000) != 0;
            if (exitKeyDown && !exitKeyWasDown)
                io_state::exit_requested = true;
            exitKeyWasDown = exitKeyDown;
        }

        if (!io_state::menu_open)
            return false;

        ImGui::SetNextWindowSize(ImVec2(760, 500), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.96f * menuAlpha);

        if (g_menuFont)
            ImGui::PushFont(g_menuFont);

        bool interacted = false;
        if (ImGui::Begin("AZURE MENU", nullptr, ImGuiWindowFlags_NoCollapse)) {
            interacted = true;
            ImDrawList* draw = ImGui::GetWindowDrawList();
            const ImVec2 pos = ImGui::GetWindowPos();
            const ImVec2 size = ImGui::GetWindowSize();
            const float t = static_cast<float>(ImGui::GetTime());
            const float pulse = 0.20f + 0.08f * static_cast<float>(std::sin(t * 1.7f));

            draw->AddRectFilledMultiColor(
                pos,
                ImVec2(pos.x + size.x, pos.y + 68.0f),
                ImGui::GetColorU32(ImVec4(0.20f + pulse, 0.15f + pulse * 0.7f, 0.07f, 0.50f)),
                ImGui::GetColorU32(ImVec4(0.10f, 0.09f, 0.08f, 0.20f)),
                ImGui::GetColorU32(ImVec4(0.06f, 0.06f, 0.07f, 0.00f)),
                ImGui::GetColorU32(ImVec4(0.18f + pulse * 0.5f, 0.14f + pulse * 0.4f, 0.07f, 0.36f))
            );
            draw->AddRect(
                pos,
                ImVec2(pos.x + size.x, pos.y + 68.0f),
                ImGui::GetColorU32(ImVec4(0.56f, 0.46f, 0.24f, 0.55f)),
                0.0f, 0, 1.0f);

            const float logoSize = 40.0f;
            const float logoPad = 12.0f;
            const float baseY = pos.y + (68.0f - logoSize) * 0.5f;
            const ImVec2 logoPos(pos.x + logoPad, baseY);
            const ImU32 accent = ImGui::GetColorU32(ImVec4(0.52f, 0.40f, 0.18f, 1.0f));
            draw->AddRectFilled(logoPos, ImVec2(logoPos.x + logoSize, logoPos.y + logoSize), accent, 8.0f);
            draw->AddRect(logoPos, ImVec2(logoPos.x + logoSize, logoPos.y + logoSize), IM_COL32(255, 255, 255, 90), 8.0f, 0, 1.0f);
            if (g_menuFont) {
                ImVec2 textSize = g_menuFont->CalcTextSizeA(22.0f, FLT_MAX, 0.0f, "AZ");
                draw->AddText(g_menuFont, 22.0f, ImVec2(logoPos.x + (logoSize - textSize.x) * 0.5f, logoPos.y + (logoSize - textSize.y) * 0.5f), IM_COL32(255, 255, 255, 255), "AZ");
                draw->AddText(g_menuFont, 18.0f, ImVec2(pos.x + 12.0f + logoSize + 10.0f, pos.y + 22.0f), IM_COL32(235, 235, 240, 255), "FN EXTERNAL");
            } else {
                draw->AddText(ImVec2(logoPos.x + 12.0f, logoPos.y + 10.0f), IM_COL32(255, 255, 255, 255), "AZ");
            }

            ImGui::BeginChild("Sidebar", ImVec2(210, 0), true);
            ImGui::TextColored(ImVec4(0.95f, 0.84f, 0.52f, 1.0f), "AZURE MENU");
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
            ImGui::TextDisabled("Refined Overlay Control");
            ImGui::PopTextWrapPos();
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (DrawSidebarButton("Aim", mainTab == 0)) mainTab = 0;
            if (DrawSidebarButton("ESP", mainTab == 1)) mainTab = 1;
            if (DrawSidebarButton("Misc", mainTab == 2)) mainTab = 2;

            const float sidebarH = ImGui::GetWindowHeight();
            ImGui::SetCursorPosY(sidebarH - 92.0f);
            ImGui::Separator();

            if (g_discord_state.have_account) {
                if (g_discord_state.need_avatar) {
                    discord_refresh_avatar();
                }

                const float avatarSize = 30.0f;
                const float panelHeight = avatarSize + 8.0f;
                ImGui::BeginChild("DiscordPanel", ImVec2(210.0f, panelHeight), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
                if (g_avatarSrv) {
                    const ImTextureID avatarId = static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(g_avatarSrv));
                    ImGui::Image(avatarId, ImVec2(avatarSize, avatarSize));
                }
                ImGui::PopStyleVar(2);

                const ImVec2 panelPos = ImGui::GetWindowPos();
                const ImVec2 namePos(panelPos.x + 4.0f + avatarSize + 6.0f, panelPos.y + 3.0f);
                draw->AddText(namePos, IM_COL32(255, 255, 255, 255), discord_display_name());

                const ImVec2 statusPos(panelPos.x + 4.0f + avatarSize + 6.0f, panelPos.y + 3.0f + 16.0f);
                draw->AddCircleFilled(ImVec2(statusPos.x - 8.0f, statusPos.y + 5.0f), 4.0f, IM_COL32(88, 101, 242, 255));
                draw->AddText(statusPos, IM_COL32(88, 101, 242, 255), "DISCORD");

                ImGui::EndChild();
                ImGui::Separator();
            }

            ImGui::SetCursorPosY(sidebarH - 46.0f);
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
            const char* menuKeyName = KeyName(GetMenuKey());
            const char* exitKeyName = KeyName(GetExitKey());
            ImGui::TextDisabled("Menu: %s", menuKeyName);
            ImGui::TextDisabled("Exit: %s", exitKeyName);
            ImGui::PopTextWrapPos();
            ImGui::EndChild();

            ImGui::SameLine();

            ImGui::BeginChild("Content", ImVec2(0, 0), true);
            if (mainTab == 0) {
                RenderAimTab();
            } else if (mainTab == 1) {
                RenderEspTab();
            } else {
                RenderMiscTab();
            }
            ImGui::EndChild();
        }
        ImGui::End();

        if (g_menuFont)
            ImGui::PopFont();

        return interacted;
    }

} // namespace Menu