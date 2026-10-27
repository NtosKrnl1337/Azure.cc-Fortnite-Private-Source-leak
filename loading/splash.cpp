// splash.cpp — azure.cc styled loading screen (black, neon beam, logo).
// Runs on its own thread while WinMain initializes; closes before overlay.
#define IMGUI_DEFINE_MATH_OPERATORS
#include "splash.h"

#include <thirdparty/imgui/imgui.h>
#include <thirdparty/imgui/imgui_impl_dx11.h>
#include <thirdparty/imgui/imgui_impl_win32.h>
#include <d3d11.h>
#include <wincodec.h>
#include <string>
#include <vector>
#include <thread>
#include <cmath>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "windowscodecs.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace splash {

static std::thread s_th;
static bool s_joined = true;
static ID3D11ShaderResourceView* s_logo = nullptr;

static ID3D11ShaderResourceView* LoadLogo(ID3D11Device* dev) {
    if (s_logo) return s_logo;
    char mp[MAX_PATH] = { 0 };
    GetModuleFileNameA(NULL, mp, MAX_PATH);
    std::string exedir = mp;
    exedir = exedir.substr(0, exedir.find_last_of("\\/") + 1);
    std::string p2 = exedir + "azurelogo.png";
    const char* paths[2] = { "C:\\Users\\xvolk\\Downloads\\azurelogo.png", p2.c_str() };
    for (int i = 0; i < 2; ++i) {
        DWORD at = GetFileAttributesA(paths[i]);
        if (at == INVALID_FILE_ATTRIBUTES || (at & FILE_ATTRIBUTE_DIRECTORY)) continue;
        IWICImagingFactory* fac = nullptr;
        if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&fac))) || !fac)
            continue;
        wchar_t wp[MAX_PATH] = { 0 };
        MultiByteToWideChar(CP_UTF8, 0, paths[i], -1, wp, MAX_PATH);
        IWICBitmapDecoder* dec = nullptr;
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
        if (srv) { s_logo = srv; break; }
    }
    return s_logo;
}

static LRESULT CALLBACK SplashProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (ImGui_ImplWin32_WndProcHandler(h, m, w, l)) return 1;
    return DefWindowProcW(h, m, w, l);
}

static void Run() {
    const int W = 480, H = 300;
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = SplashProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = L"AzureSplash";
    RegisterClassExW(&wc);
    int sx = (GetSystemMetrics(SM_CXSCREEN) - W) / 2;
    int sy = (GetSystemMetrics(SM_CYSCREEN) - H) / 2;
    HWND hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, L"AzureSplash", L"",
        WS_POPUP | WS_VISIBLE, sx, sy, W, H, nullptr, nullptr, wc.hInstance, nullptr);
    if (!hwnd) return;

    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = W;
    sd.BufferDesc.Height = H;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    ID3D11Device* dev = nullptr;
    ID3D11DeviceContext* ctx = nullptr;
    IDXGISwapChain* sw = nullptr;
    const D3D_FEATURE_LEVEL lv[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    D3D_FEATURE_LEVEL got;
    if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        lv, 2, D3D11_SDK_VERSION, &sd, &sw, &dev, &got, &ctx))) {
        DestroyWindow(hwnd);
        return;
    }
    ID3D11Texture2D* back = nullptr;
    ID3D11RenderTargetView* rtv = nullptr;
    if (FAILED(sw->GetBuffer(0, IID_PPV_ARGS(&back))) || !back) {
        sw->Release(); ctx->Release(); dev->Release(); DestroyWindow(hwnd);
        return;
    }
    dev->CreateRenderTargetView(back, nullptr, &rtv);
    back->Release();

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.Fonts->AddFontDefault();
    ImGui::StyleColorsDark();
    ImGuiStyle& st = ImGui::GetStyle();
    st.WindowRounding = 8;
    st.FrameRounding = 6;
    st.Colors[ImGuiCol_WindowBg] = ImVec4(0.008f, 0.008f, 0.01f, 1.0f);
    st.Colors[ImGuiCol_Text] = ImVec4(0.95f, 0.95f, 0.96f, 1.0f);
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(dev, ctx);
    LoadLogo(dev);

    static const char* stageTxt[] = {
        "Initializing...", "Loading driver...", "Open Fortnite now",
        "Reading game memory...", "Starting overlay..."
    };

    MSG msg{};
    while (!done.load()) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2((float)W, (float)H));
        ImGui::Begin("##splash", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);
        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 wp = ImGui::GetWindowPos();
            float t = (float)ImGui::GetTime();

            // neon beam around border
            {
                float per = 2.0f * (W + H);
                float head = fmodf(t * 260.0f, per);
                auto pt = [&](float dd) {
                    float d2 = fmodf(dd, per); if (d2 < 0) d2 += per;
                    if (d2 < W) return ImVec2(wp.x + d2, wp.y);
                    else if (d2 < W + H) return ImVec2(wp.x + W, wp.y + (d2 - W));
                    else if (d2 < 2.0f * W + H) return ImVec2(wp.x + W - (d2 - W - H), wp.y + H);
                    else return ImVec2(wp.x, wp.y + H - (d2 - 2.0f * W - H));
                };
                dl->AddRect(wp, ImVec2(wp.x + W, wp.y + H), IM_COL32(255, 255, 255, 40), 8.0f, 0, 1.5f);
                for (float s = 90.0f; s > 0.0f; s -= 5.0f) {
                    ImVec2 a = pt(head - s), b = pt(head - s + 5.0f);
                    int al = 90 + (int)(165.0f * (1.0f - s / 90.0f));
                    dl->AddLine(a, b, IM_COL32(255, 255, 255, al), 2.5f);
                }
            }

            // logo + title
            if (s_logo)
                ImGui::SetCursorPos(ImVec2((W - 200) * 0.5f, 34));
            else
                ImGui::SetCursorPos(ImVec2((W - 200) * 0.5f, 34));
            if (s_logo) {
                ImGui::Image((ImTextureID)s_logo, ImVec2(44, 44));
                ImGui::SameLine(0, 12);
                ImGui::BeginGroup();
                ImGui::SetWindowFontScale(1.5f);
                ImGui::TextUnformatted("azure.cc");
                ImGui::SetWindowFontScale(1.0f);
                ImGui::TextColored(ImVec4(0.45f, 0.47f, 0.52f, 1.0f), "Fortnite Private");
                ImGui::EndGroup();
            } else {
                ImGui::SetWindowFontScale(1.6f);
                ImVec2 tsz = ImGui::CalcTextSize("azure.cc");
                ImGui::SetCursorPos(ImVec2((W - tsz.x) * 0.5f, 44));
                ImGui::TextUnformatted("azure.cc");
                ImGui::SetWindowFontScale(1.0f);
            }

            // status
            int sg = stage.load();
            if (sg < 0) sg = 0;
            if (sg > 4) sg = 4;
            ImVec2 ssz = ImGui::CalcTextSize(stageTxt[sg]);
            ImGui::SetCursorPos(ImVec2((W - ssz.x) * 0.5f, 150));
            ImGui::TextColored(ImVec4(0.75f, 0.76f, 0.80f, 1.0f), "%s", stageTxt[sg]);

            // progress: stage fill + sweep
            {
                ImVec2 b0(wp.x + (W - 260) * 0.5f, wp.y + 190);
                dl->AddRectFilled(b0, ImVec2(b0.x + 260, b0.y + 3), IM_COL32(255, 255, 255, 28), 1.0f);
                dl->AddRectFilled(b0, ImVec2(b0.x + 260.0f * (sg / 4.0f), b0.y + 3), IM_COL32(255, 255, 255, 200), 1.0f);
                float span = 260.0f + 60.0f;
                float hx = fmodf(t * 220.0f, span) - 30.0f;
                float x0 = b0.x + hx - 25.0f, x1 = b0.x + hx + 25.0f;
                if (x0 < b0.x) x0 = b0.x;
                if (x1 > b0.x + 260) x1 = b0.x + 260;
                if (x1 > x0) dl->AddRectFilled(ImVec2(x0, b0.y - 1), ImVec2(x1, b0.y + 4), IM_COL32(255, 255, 255, 120), 1.0f);
            }

            ImVec2 fsz = ImGui::CalcTextSize("made by x*v*r");
            ImGui::SetCursorPos(ImVec2((W - fsz.x) * 0.5f, 262));
            ImGui::SetWindowFontScale(0.85f);
            ImGui::TextColored(ImVec4(0.35f, 0.36f, 0.40f, 1.0f), "made by x*v*r");
            ImGui::SetWindowFontScale(1.0f);

            // drag via top zone
            ImGui::SetCursorScreenPos(wp);
            ImGui::InvisibleButton("##drag", ImVec2((float)W, 30));
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
                POINT p;
                GetCursorPos(&p);
                SetWindowPos(hwnd, nullptr, p.x - W / 2, p.y - 15, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            }
        }
        ImGui::End();
        ImGui::Render();
        const float cc[4] = { 0, 0, 0, 1 };
        ctx->OMSetRenderTargets(1, &rtv, nullptr);
        ctx->ClearRenderTargetView(rtv, cc);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        sw->Present(1, 0);
    }

    if (s_logo) { s_logo->Release(); s_logo = nullptr; }
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    if (rtv) rtv->Release();
    sw->Release(); ctx->Release(); dev->Release();
    DestroyWindow(hwnd);
    UnregisterClassW(L"AzureSplash", GetModuleHandleW(nullptr));
}

void Start() {
    done.store(false);
    s_joined = false;
    s_th = std::thread(Run);
}

void Stop() {
    done.store(true);
    if (!s_joined && s_th.joinable()) {
        s_th.join();
        s_joined = true;
    }
}

} // namespace splash
