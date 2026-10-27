// MenuZyrex.cpp — azure.cc menu for the Fortnite cheat.
// Zyrex-style framework (sidebar tabs, subtabs, ImAdd widgets) bound to the
// REAL cheat config (Menu::GetConfig). Backend (sync, autosave, keybinds)
// stays in Menu.cpp; only the rendered window is replaced.
#define IMGUI_DEFINE_MATH_OPERATORS
#include <src/rendering/menu_cs.h>
#include <src/cache/index.h>
#include <src/rendering/zyrex/zyrex_addons.h>

#include <d3d11.h>
#include <wincodec.h>
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace Menu {

static int zxPage = 0;
static int zxSub[4] = { 0, 0, 0, 0 };
static ImVec4 zxAccent = ImVec4(0.34f, 0.745f, 0.918f, 1.0f);
static ID3D11ShaderResourceView* zxLogo = nullptr;
static bool zxLogoTried = false;
static int* zxCapturing = nullptr;
static char zxStatus[128] = { 0 };

static const char* VkName(int vk) {
    switch (vk) {
    case 0: return "None";
    case VK_LBUTTON: return "LMB";
    case VK_RBUTTON: return "RMB";
    case VK_MBUTTON: return "MMB";
    case VK_XBUTTON1: return "Mouse4";
    case VK_XBUTTON2: return "Mouse5";
    case VK_INSERT: return "INSERT";
    case VK_DELETE: return "DELETE";
    case VK_HOME: return "HOME";
    case VK_END: return "END";
    case VK_ESCAPE: return "ESC";
    case VK_SHIFT: return "SHIFT";
    case VK_CONTROL: return "CTRL";
    case VK_MENU: return "ALT";
    case VK_SPACE: return "SPACE";
    case VK_CAPITAL: return "CAPS";
    default: break;
    }
    static char b[16];
    if (vk >= 'A' && vk <= 'Z' || vk >= '0' && vk <= '9') {
        b[0] = (char)vk; b[1] = 0;
        return b;
    }
    sprintf_s(b, "0x%02X", vk);
    return b;
}

static void DrawVkPicker(const char* id, int* vk) {
    const char* label = (zxCapturing == vk) ? "..." : VkName(*vk);
    char btn[64];
    sprintf_s(btn, "%s##%s", label, id);
    if (ImGui::Button(btn, ImVec2(110, 0))) {
        for (int v = 1; v < 255; ++v) (void)GetAsyncKeyState(v); // flush stale edges
        zxCapturing = vk;
    }
}

static void CkBox(const char* label, bool* v) {
    CkBox(label, v);
    ImGui::SameLine();
    ImGui::TextColored(*v ? ImVec4(0.35f, 0.85f, 0.45f, 1.0f) : ImVec4(0.45f, 0.45f, 0.48f, 1.0f), *v ? "ON" : "OFF");
}

static void PollVkCapture() {
    if (!zxCapturing) return;
    if (GetAsyncKeyState(VK_ESCAPE) & 1) { zxCapturing = nullptr; return; }
    for (int v = 1; v < 255; ++v) {
        if (v == VK_ESCAPE) continue;
        if (GetAsyncKeyState(v) & 1) { *zxCapturing = v; zxCapturing = nullptr; break; }
    }
}

static ID3D11ShaderResourceView* LoadLogoTexture() {
    if (zxLogo || zxLogoTried) return zxLogo;
    zxLogoTried = true;
    char mp[MAX_PATH] = { 0 };
    GetModuleFileNameA(NULL, mp, MAX_PATH);
    std::string exep = mp;
    std::string exedir = exep.substr(0, exep.find_last_of("\\/") + 1);
    const char* cands[] = {
        "C:\\Users\\xvolk\\Downloads\\azurelogo.png",
        nullptr
    };
    std::string p2 = exedir + "azurelogo.png";
    const char* paths[2] = { cands[0], p2.c_str() };
    ID3D11Device* dev = (ID3D11Device*)g_renderDevice;
    if (!dev) return nullptr;
    for (int i = 0; i < 2; ++i) {
        DWORD at = GetFileAttributesA(paths[i]);
        if (at == INVALID_FILE_ATTRIBUTES || (at & FILE_ATTRIBUTE_DIRECTORY)) continue;
        IWICImagingFactory* fac = nullptr;
        if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&fac))) || !fac)
            continue;
        IWICBitmapDecoder* dec = nullptr;
        wchar_t wp[MAX_PATH] = { 0 };
        MultiByteToWideChar(CP_UTF8, 0, paths[i], -1, wp, MAX_PATH);
        ID3D11ShaderResourceView* srv = nullptr;
        if (SUCCEEDED(fac->CreateDecoderFromFilename(wp, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &dec)) && dec) {
            IWICBitmapFrameDecode* fr = nullptr;
            if (SUCCEEDED(dec->GetFrame(0, &fr)) && fr) {
                IWICFormatConverter* cv = nullptr;
                if (SUCCEEDED(fac->CreateFormatConverter(&cv)) && cv) {
                    if (SUCCEEDED(cv->Initialize(fr, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) {
                        UINT w = 0, h = 0;
                        cv->GetSize(&w, &h);
                        if (w && h && w < 2048 && h < 2048) {
                            std::vector<unsigned char> px((size_t)w * h * 4);
                            if (SUCCEEDED(cv->CopyPixels(nullptr, w * 4, (UINT)px.size(), px.data()))) {
                                D3D11_TEXTURE2D_DESC td{};
                                td.Width = w; td.Height = h;
                                td.MipLevels = 1; td.ArraySize = 1;
                                td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                                td.SampleDesc.Count = 1;
                                td.Usage = D3D11_USAGE_DEFAULT;
                                td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                                D3D11_SUBRESOURCE_DATA sd{};
                                sd.pSysMem = px.data();
                                sd.SysMemPitch = w * 4;
                                ID3D11Texture2D* tx = nullptr;
                                if (SUCCEEDED(dev->CreateTexture2D(&td, &sd, &tx)) && tx) {
                                    dev->CreateShaderResourceView(tx, nullptr, &srv);
                                    tx->Release();
                                }
                            }
                        }
                    }
                    cv->Release();
                }
                fr->Release();
            }
            dec->Release();
        }
        fac->Release();
        if (srv) { zxLogo = srv; break; }
    }
    return zxLogo;
}

static void ApplyZyrexTheme() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 7;
    s.ChildRounding = 5;
    s.FrameRounding = 4;
    s.PopupRounding = 3;
    s.WindowBorderSize = 1;
    s.ChildBorderSize = 1;
    s.FrameBorderSize = 0;
    s.WindowPadding = ImVec2(16, 16);
    s.ItemSpacing = s.WindowPadding;
    s.ItemInnerSpacing = ImVec2(8, 6);
    s.FramePadding = ImVec2(12, 8);
    s.CellPadding = ImVec2(4, 2);
    s.ScrollbarSize = 6.0f;
    s.Colors[ImGuiCol_WindowBg] = ImAdd::HexToColorVec4(0x0a0a0f);
    s.Colors[ImGuiCol_ChildBg] = ImAdd::HexToColorVec4(0x12121c, 0.85f);
    s.Colors[ImGuiCol_PopupBg] = ImAdd::HexToColorVec4(0x12121c);
    s.Colors[ImGuiCol_TitleBg] = ImAdd::HexToColorVec4(0x12121c);
    s.Colors[ImGuiCol_TitleBgActive] = s.Colors[ImGuiCol_TitleBg];
    s.Colors[ImGuiCol_Text] = ImAdd::HexToColorVec4(0xdadada);
    s.Colors[ImGuiCol_TextDisabled] = ImAdd::HexToColorVec4(0x555555, 0.9f);
    s.Colors[ImGuiCol_CheckMark] = zxAccent;
    s.Colors[ImGuiCol_Border] = ImAdd::HexToColorVec4(0x23232e, 0.9f);
    s.Colors[ImGuiCol_Separator] = s.Colors[ImGuiCol_Border];
    s.Colors[ImGuiCol_Header] = ImVec4(zxAccent.x, zxAccent.y, zxAccent.z, 0.55f);
    s.Colors[ImGuiCol_HeaderHovered] = ImVec4(zxAccent.x, zxAccent.y, zxAccent.z, 0.8f);
    s.Colors[ImGuiCol_HeaderActive] = zxAccent;
    s.Colors[ImGuiCol_Tab] = ImVec4(0.09f, 0.09f, 0.12f, 1.0f);
    s.Colors[ImGuiCol_TabHovered] = ImVec4(zxAccent.x, zxAccent.y, zxAccent.z, 0.35f);
    s.Colors[ImGuiCol_TabActive] = ImVec4(zxAccent.x, zxAccent.y, zxAccent.z, 0.6f);
    s.Colors[ImGuiCol_Button] = ImVec4(0.10f, 0.10f, 0.13f, 1.0f);
    s.Colors[ImGuiCol_ButtonHovered] = ImVec4(zxAccent.x, zxAccent.y, zxAccent.z, 0.45f);
    s.Colors[ImGuiCol_ButtonActive] = zxAccent;
    s.Colors[ImGuiCol_FrameBg] = ImVec4(0.07f, 0.07f, 0.10f, 1.0f);
    s.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.10f, 0.10f, 0.14f, 1.0f);
    s.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.12f, 0.12f, 0.16f, 1.0f);
    s.Colors[ImGuiCol_SliderGrab] = zxAccent;
    s.Colors[ImGuiCol_SliderGrabActive] = zxAccent;
}

static void DrawBindRow(const char* label, KeyBind& b) {
    ImGui::TextUnformatted(label);
    ImGui::SameLine(180);
    const char* modes[] = { "Always on", "Hold" };
    char mid[32];
    sprintf_s(mid, "##mode_%s", label);
    ImGui::PushItemWidth(110);
    ImGui::Combo(mid, &b.mode, modes, 2);
    ImGui::PopItemWidth();
    ImGui::SameLine();
    DrawVkPicker(label, &b.key);
}

static void DrawEspSet(const char* prefix, bool& box, bool& name, bool& health, bool& hpText,
                       bool& tracer, bool& skeleton, bool& wText, bool& wIcon,
                       float* col, float* tracerCol, float* skelCol, float* nameCol,
                       float* hpCol, float* wCol) {
    char id[64];
    float gw2 = ImGui::GetContentRegionAvail().x;
    float left = gw2 * 0.52f;
    ImGui::BeginGroup();
    if (ImAdd::BeginChild(prefix, ImVec2(left, 0))) {
        sprintf_s(id, "%s_box", prefix); CkBox("Box", &box);
        sprintf_s(id, "%s_name", prefix); CkBox("Name", &name);
        sprintf_s(id, "%s_hp", prefix); CkBox("Health bar", &health);
        sprintf_s(id, "%s_hpt", prefix); CkBox("HP text", &hpText);
        sprintf_s(id, "%s_tr", prefix); CkBox("Tracer", &tracer);
        sprintf_s(id, "%s_sk", prefix); CkBox("Skeleton", &skeleton);
        sprintf_s(id, "%s_wt", prefix); CkBox("Weapon text", &wText);
        sprintf_s(id, "%s_wi", prefix); CkBox("Weapon icon", &wIcon);
    }
    ImAdd::EndChild();
    ImGui::EndGroup();
    ImGui::SameLine();
    ImGui::BeginGroup();
    if (ImAdd::BeginChild("c", ImVec2(0, 0))) {
        ImAdd::SeparatorText("Colors");
        sprintf_s(id, "%s main", prefix); ImAdd::ColorEdit4(id, col);
        sprintf_s(id, "%s tracer", prefix); ImAdd::ColorEdit4(id, tracerCol);
        sprintf_s(id, "%s skeleton", prefix); ImAdd::ColorEdit4(id, skelCol);
        sprintf_s(id, "%s name", prefix); ImAdd::ColorEdit4(id, nameCol);
        sprintf_s(id, "%s health", prefix); ImAdd::ColorEdit4(id, hpCol);
        sprintf_s(id, "%s weapon", prefix); ImAdd::ColorEdit4(id, wCol);
    }
    ImAdd::EndChild();
    ImGui::EndGroup();
}

static bool RenderZyrex() {
    static bool themed = false;
    if (!themed) { ApplyZyrexTheme(); themed = true; }
    Config& cfg = GetConfig();
    PollVkCapture();

    ImGuiStyle& style = ImGui::GetStyle();
    ImGuiIO& io = ImGui::GetIO();

    ImGui::SetNextWindowSize(ImVec2(760, 560), ImGuiCond_FirstUseEver);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    bool open = ImGui::Begin("azure.cc", (bool*)0,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar(2);
    if (!open) { ImGui::End(); return false; }

    const float headerH = 64.0f;
    ImVec2 wpos = ImGui::GetWindowPos();
    ImVec2 wsize = ImGui::GetWindowSize();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(wpos, ImVec2(wpos.x + wsize.x, wpos.y + headerH), ImGui::GetColorU32(ImGuiCol_TitleBg), 7.0f, ImDrawFlags_RoundCornersTop);
    dl->AddRectFilled(ImVec2(wpos.x, wpos.y + headerH), ImVec2(wpos.x + wsize.x, wpos.y + wsize.y), ImGui::GetColorU32(ImGuiCol_WindowBg), 7.0f, ImDrawFlags_RoundCornersBottom);
    dl->AddRect(wpos, ImVec2(wpos.x + wsize.x, wpos.y + wsize.y), ImGui::GetColorU32(ImGuiCol_Border), 7.0f, 0, 1.0f);

    // header: logo + azure.cc + close (zyrex style)
    {
        ID3D11ShaderResourceView* logo = LoadLogoTexture();
        ImGui::SetCursorScreenPos(ImVec2(wpos.x + 14, wpos.y + 10));
        if (ImGui::BeginChild("zxhead", ImVec2(wsize.x - 28, headerH - 10), false, ImGuiWindowFlags_NoBackground)) {
            if (logo)
                ImGui::Image((ImTextureID)logo, ImVec2(40, 40));
            else {
                ImVec2 lp = ImGui::GetCursorScreenPos();
                ImDrawList* hdl = ImGui::GetWindowDrawList();
                hdl->AddRectFilled(lp, ImVec2(lp.x + 40, lp.y + 40), IM_COL32(255, 255, 255, 235), 8.0f);
                ImVec2 asz = ImGui::CalcTextSize("A");
                hdl->AddText(ImVec2(lp.x + (40 - asz.x) * 0.5f, lp.y + (40 - asz.y) * 0.5f), IM_COL32(10, 10, 15, 255), "A");
                ImGui::Dummy(ImVec2(40, 40));
            }
            ImGui::SameLine(0, 12);
            ImGui::BeginGroup();
            ImGui::SetWindowFontScale(1.4f);
            ImGui::TextUnformatted("azure.cc");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::TextColored(ImVec4(0.45f, 0.47f, 0.52f, 1.0f), "Fortnite Private");
            ImGui::EndGroup();
            ImGui::SameLine(wsize.x - 28 - 24);
            if (ImAdd::ButtonXMark("zxclose", ImVec2(20, 20)))
                io_state::menu_open = false;
        }
        ImGui::EndChild();
        dl->AddLine(ImVec2(wpos.x + 14, wpos.y + headerH), ImVec2(wpos.x + wsize.x - 14, wpos.y + headerH), ImGui::GetColorU32(ImGuiCol_Border), 1.0f);
    }

    struct TabDef { const char* label; const char* subs[4]; int nsub; };
    static const TabDef tabs[] = {
        { "Legit", { "Aimbot", "Trigger" }, 2 },
        { "Rage", { nullptr }, 0 },
        { "Visuals", { "Enemy", "Team", "Loot", "Style" }, 4 },
        { "Misc", { "General", "Config" }, 2 },
    };

    // menubar
    ImGui::SetCursorScreenPos(ImVec2(wpos.x + 14, wpos.y + headerH + 8));
    if (ImGui::BeginChild("zxbar", ImVec2(wsize.x - 28, ImGui::GetFrameHeight() + 8), false, ImGuiWindowFlags_NoBackground)) {
        for (int i = 0; i < 4; ++i) {
            ImAdd::TabIcon((ImTextureID)0, tabs[i].label, &zxPage, i, true);
            if (i < 3) ImGui::SameLine();
        }
    }
    ImGui::EndChild();

    const float sidebarW = 150.0f;
    const float pageY = wpos.y + headerH + 8 + ImGui::GetFrameHeight() + 16;
    const float pageH = wpos.y + wsize.y - pageY - 14;
    bool hasSub = tabs[zxPage].nsub > 0;

    if (hasSub) {
        ImGui::SetCursorScreenPos(ImVec2(wpos.x + 14, pageY));
        if (ImAdd::BeginChild("zxside", ImVec2(sidebarW, pageH), true)) {
            for (int i = 0; i < tabs[zxPage].nsub; ++i)
                ImAdd::TabIcon((ImTextureID)0, tabs[zxPage].subs[i], &zxSub[zxPage], i, false, ImVec2(sidebarW - 28, 0));
        }
        ImAdd::EndChild();
    }

    ImGui::SetCursorScreenPos(ImVec2(wpos.x + 14 + (hasSub ? sidebarW + 10 : 0), pageY));
    if (ImAdd::BeginChild("zxpage", ImVec2(wsize.x - 28 - (hasSub ? sidebarW + 10 : 0), pageH), true)) {
        const int sub = hasSub ? zxSub[zxPage] : 0;
        const float gw = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        if (zxPage == 0 && sub == 0) {
            // Combat / Aimbot
            ImGui::BeginGroup();
            if (ImAdd::BeginChild("zx_aim_a", ImVec2(gw, 0))) {
                ImAdd::ToggleButton("Enabled", &cfg.aimEnabled);
                DrawBindRow("Aim bind", cfg.aimBind);
                CkBox("Visible only", &cfg.aimVisibleOnly);
            }
            ImAdd::EndChild();
            ImGui::EndGroup();
            ImGui::SameLine();
            ImGui::BeginGroup();
            if (ImAdd::BeginChild("zx_aim_b", ImVec2(0, 0))) {
                ImAdd::SliderFloat("FOV", &cfg.aimFov, 20.0f, 300.0f, "%.0f");
                ImAdd::SliderFloat("Smooth", &cfg.aimSmooth, 1.0f, 20.0f, "%.1f");
                ImAdd::SliderInt("Target bone", &cfg.aimTargetBone, 0, 30);
                CkBox("Show FOV", &cfg.aimShowFov);
                ImAdd::ColorEdit4("FOV color", cfg.aimFovColor);
            }
            ImAdd::EndChild();
            ImGui::EndGroup();
        } else if (zxPage == 0 && sub == 1) {
            // Combat / Trigger
            ImGui::BeginGroup();
            if (ImAdd::BeginChild("zx_trg_a", ImVec2(gw, 0))) {
                ImAdd::ToggleButton("Enabled", &cfg.triggerEnabled);
                DrawBindRow("Trigger bind", cfg.triggerBind);
                CkBox("Visible only", &cfg.triggerVisibleOnly);
                CkBox("Aim assist", &cfg.triggerAimAssist);
            }
            ImAdd::EndChild();
            ImGui::EndGroup();
            ImGui::SameLine();
            ImGui::BeginGroup();
            if (ImAdd::BeginChild("zx_trg_b", ImVec2(0, 0))) {
                ImAdd::SliderInt("Target bone", &cfg.triggerTargetBone, 0, 30);
                ImAdd::SliderFloat("Tap time ms", &cfg.triggerTapTimeMs, 0.0f, 50.0f, "%.1f");
                ImAdd::SliderFloat("Cooldown ms", &cfg.triggerCooldownMs, 0.0f, 500.0f, "%.0f");
            }
            ImAdd::EndChild();
            ImGui::EndGroup();
        } else if (zxPage == 1) {
            // Rage exploits (10 features)
            if (ImGui::Button("Legit preset", ImVec2(200, 0))) {
                cfg.aimEnabled = true; cfg.aimFov = 80.0f; cfg.aimSmooth = 8.0f; cfg.aimTargetBone = 0; cfg.aimVisibleOnly = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Rage preset", ImVec2(200, 0))) {
                cfg.aimEnabled = true; cfg.aimFov = 300.0f; cfg.aimSmooth = 1.0f; cfg.aimTargetBone = 0; cfg.aimVisibleOnly = false;
            }
            CkBox("Shoot through walls", &cfg.rageWalls);
            CkBox("Speed hack", &cfg.rageSpeed);
            ImAdd::SliderFloat("Speed multiplier", &cfg.rageSpeedVal, 0.5f, 3.0f, "%.2f");
            CkBox("Infinite ammo", &cfg.rageInfAmmo);
            CkBox("No reload", &cfg.rageNoReload);
            CkBox("Rapid fire", &cfg.rageRapidFire);
            CkBox("FOV changer", &cfg.rageFovOn);
            ImAdd::SliderFloat("Custom FOV", &cfg.rageFovVal, 60.0f, 130.0f, "%.0f");
            CkBox("Spinbot", &cfg.rageSpin);
            ImGui::TextUnformatted("Zoom key");
            ImGui::SameLine(180);
            DrawVkPicker("zoomkey", &cfg.rageZoomKey);
            ImAdd::SliderFloat("Zoom FOV", &cfg.rageZoomFov, 10.0f, 60.0f, "%.0f");
            ImGui::TextUnformatted("Panic key");
            ImGui::SameLine(180);
            DrawVkPicker("panickey", &cfg.ragePanicKey);
            ImAdd::CheckBox("Triggerbot", &cfg.triggerEnabled);
        } else if (zxPage == 2 && sub == 0) {
            DrawEspSet("enemy", cfg.espEnemyBox, cfg.espEnemyName, cfg.espEnemyHealth, cfg.espEnemyHpText,
                       cfg.espEnemyTracer, cfg.espEnemySkeleton, cfg.espEnemyWeaponText, cfg.espEnemyWeaponIcon,
                       cfg.espEnemyColor, cfg.espEnemyTracerColor, cfg.espEnemySkeletonColor,
                       cfg.espEnemyNameColor, cfg.espEnemyHealthColor, cfg.espEnemyWeaponColor);
        } else if (zxPage == 2 && sub == 1) {
            DrawEspSet("team", cfg.espTeamBox, cfg.espTeamName, cfg.espTeamHealth, cfg.espTeamHpText,
                       cfg.espTeamTracer, cfg.espTeamSkeleton, cfg.espTeamWeaponText, cfg.espTeamWeaponIcon,
                       cfg.espTeamColor, cfg.espTeamTracerColor, cfg.espTeamSkeletonColor,
                       cfg.espTeamNameColor, cfg.espTeamHealthColor, cfg.espTeamWeaponColor);
        } else if (zxPage == 2 && sub == 2) {
            // Visuals / Loot
            ImAdd::ToggleButton("Chests", &cfg.lootChests);
            ImAdd::ColorEdit4("Chest color", cfg.lootChestColor);
            ImAdd::ToggleButton("Pickups", &cfg.lootPickups);
            ImAdd::ToggleButton("Supply drops", &cfg.lootDrops);
            ImAdd::SliderFloat("Max distance", &cfg.lootMaxDist, 25.0f, 300.0f, "%.0fm");
            CkBox("Show distance", &cfg.lootShowDist);
        } else if (zxPage == 2 && sub == 3) {
            CkBox("Show distance", &cfg.espShowDistance);
            CkBox("Show platform", &cfg.espShowPlatform);
            CkBox("Show rank", &cfg.espShowRank);
            CkBox("Show ammo", &cfg.espShowAmmo);
            const char* boxes[] = { "Full box", "Corner box" };
            ImAdd::Combo("Box type", &cfg.espBoxType, { boxes[0], boxes[1] });
            ImAdd::SliderFloat("Box thickness", &cfg.espBoxThickness, 1.0f, 5.0f, "%.1f");
            CkBox("Box outline", &cfg.espBoxOutline);
            ImAdd::SliderFloat("Skeleton thickness", &cfg.espSkeletonThickness, 1.0f, 5.0f, "%.1f");
            ImAdd::SliderFloat("Tracer thickness", &cfg.espTracerThickness, 1.0f, 5.0f, "%.1f");
            ImAdd::SliderFloat("Text size", &cfg.espTextSize, 8.0f, 30.0f, "%.0f");
            CkBox("Text outline", &cfg.espTextOutline);
            CkBox("FOV triangles", &cfg.espFovTriangles);
            ImAdd::SliderFloat("Triangle size", &cfg.espFovTriangleSize, 4.0f, 30.0f, "%.0f");
        } else if (zxPage == 3 && sub == 0) {
            CkBox("Watermark", &cfg.watermarkEnabled);
            ImGui::TextUnformatted("Menu key");
            ImGui::SameLine(180);
            DrawVkPicker("menukey", &cfg.menuKey);
            ImGui::TextUnformatted("Exit key");
            ImGui::SameLine(180);
            DrawVkPicker("exitkey", &cfg.exitKey);
            ImAdd::ColorEdit4("Accent color", (float*)&zxAccent);
            if (ImGui::Button("Unload cheat", ImVec2(200, 0)))
                io_state::exit_requested = true;
        } else if (zxPage == 3 && sub == 1) {
            ImGui::TextWrapped("Config autosaves on exit and reloads on start.");
            if (ImGui::Button("Save now", ImVec2(200, 0))) {
                SaveAutosave();
                strcpy_s(zxStatus, "saved");
            }
            if (zxStatus[0]) {
                ImGui::SameLine();
                ImGui::TextDisabled("%s", zxStatus);
            }
        }
    }
    ImAdd::EndChild();

    ImGui::End();
    return true;
}

bool Render() {
    static float menuAlpha = 0.0f;
    menuAlpha = (std::min)(menuAlpha + ImGui::GetIO().DeltaTime * 3.2f, 1.0f);

    sync();

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

    return RenderZyrex();
}

} // namespace Menu
