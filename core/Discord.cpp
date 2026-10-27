#include "Discord.h"
#include <src/rendering/menu_cs.h>
#include <d3d11.h>
#include <Windows.h>
#include <winhttp.h>
#include <wincodec.h>
#include <cstring>
#include <thread>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "windowscodecs.lib")

DiscordState g_discord_state{ false, false };

namespace Discord {
    std::atomic<bool> have_account{ false };
    std::atomic<bool> need_avatar{ false };
    std::mutex mutex;
    char name[64] = "Discord";
    std::vector<unsigned char> avatar_png;

    static bool JsonGetString(const std::string& json, const char* key, std::string& out);
    static bool DiscordPipe(const wchar_t* pipe_name, std::string& out);
    static bool HttpGetBytes(const std::wstring& host, const std::wstring& path, std::vector<unsigned char>& out);
    static void WorkerFunc();

    void StartWorker() {
        static bool started = false;
        if (started) return;
        started = true;
        std::thread(WorkerFunc).detach();
    }

    void Shutdown() {
        std::lock_guard<std::mutex> lock(mutex);
        avatar_png.clear();
        have_account.store(false);
        need_avatar.store(false);
        g_discord_state.have_account = false;
        g_discord_state.need_avatar = false;
    }

    ID3D11ShaderResourceView* CreateAvatarTexture(ID3D11Device* device, bool circle_mask) {
        if (!device) return nullptr;

        std::vector<unsigned char> png;
        {
            std::lock_guard<std::mutex> lock(mutex);
            png = avatar_png;
        }
        if (png.empty()) return nullptr;

        ID3D11ShaderResourceView* srv = nullptr;
        IWICImagingFactory* factory = nullptr;
        IWICStream* stream = nullptr;
        IWICBitmapDecoder* decoder = nullptr;
        IWICBitmapFrameDecode* frame = nullptr;
        IWICFormatConverter* converter = nullptr;
        UINT src_w = 0, src_h = 0;

        do {
            HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
            if (FAILED(hr)) break;
            hr = factory->CreateStream(&stream);
            if (FAILED(hr)) break;
            hr = stream->InitializeFromMemory(avatar_png.data(), (DWORD)avatar_png.size());
            if (FAILED(hr)) break;
            hr = factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder);
            if (FAILED(hr)) break;
            hr = decoder->GetFrame(0, &frame);
            if (FAILED(hr)) break;
            hr = frame->GetSize(&src_w, &src_h);
            if (FAILED(hr) || src_w == 0 || src_h == 0) break;

            hr = factory->CreateFormatConverter(&converter);
            if (FAILED(hr)) break;
            hr = converter->Initialize(frame, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0.f, WICBitmapPaletteTypeCustom);
            if (FAILED(hr)) break;

            D3D11_TEXTURE2D_DESC desc{};
            desc.Width = src_w;
            desc.Height = src_h;
            desc.MipLevels = 1;
            desc.ArraySize = 1;
            desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

            std::vector<BYTE> pixels(src_w * src_h * 4);
            hr = converter->CopyPixels(nullptr, src_w * 4, (UINT)pixels.size(), pixels.data());
            if (FAILED(hr)) break;

            if (circle_mask) {
                const float cx = (src_w - 1) * 0.5f;
                const float cy = (src_h - 1) * 0.5f;
                const float r = (src_w < src_h ? src_w : src_h) * 0.5f - 1.0f;
                for (UINT py = 0; py < src_h; ++py) {
                    BYTE* row = pixels.data() + py * src_w * 4;
                    for (UINT px = 0; px < src_w; ++px) {
                        const float dx = (float)px - cx;
                        const float dy = (float)py - cy;
                        if (dx * dx + dy * dy > r * r)
                            row[px * 4 + 3] = 0;
                    }
                }
            }

            D3D11_SUBRESOURCE_DATA init{};
            init.pSysMem = pixels.data();
            init.SysMemPitch = src_w * 4;

            ID3D11Texture2D* tex = nullptr;
            hr = device->CreateTexture2D(&desc, &init, &tex);
            if (FAILED(hr) || !tex) break;

            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
            srvDesc.Format = desc.Format;
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = 1;
            hr = device->CreateShaderResourceView(tex, &srvDesc, &srv);
            tex->Release();
            if (FAILED(hr)) break;
        } while (0);

        if (converter) converter->Release();
        if (frame) frame->Release();
        if (decoder) decoder->Release();
        if (stream) stream->Release();
        if (factory) factory->Release();
        return srv;
    }

    static bool JsonGetString(const std::string& json, const char* key, std::string& out) {
        const std::string pat = "\"" + std::string(key) + "\"";
        const size_t p = json.find(pat);
        if (p == std::string::npos) return false;

        size_t q = p + pat.size();
        while (q < json.size() && (json[q] == ' ' || json[q] == '\t' || json[q] == ':')) q++;
        if (q >= json.size() || json[q] != '"') return false;

        q++;
        const size_t e = json.find('"', q);
        if (e == std::string::npos) return false;

        out = json.substr(q, e - q);
        return true;
    }

    static bool ReadFrame(HANDLE h, DWORD& op, std::string& payload, DWORD timeoutMs) {
        DWORD start = GetTickCount();
        DWORD avail = 0;
        for (;;) {
            if (!PeekNamedPipe(h, NULL, 0, NULL, &avail, NULL)) return false;
            if (avail >= 8) break;
            if (GetTickCount() - start > timeoutMs) return false;
            Sleep(50);
        }
        unsigned char head[8];
        DWORD rd = 0;
        if (!ReadFile(h, head, 8, &rd, NULL) || rd != 8) return false;
        DWORD len = 0;
        memcpy(&op, head, 4);
        memcpy(&len, head + 4, 4);
        if (len == 0 || len > (1 << 20)) return false;
        payload.resize(len);
        DWORD got = 0;
        while (got < len) {
            DWORD chunk = 0;
            if (!ReadFile(h, &payload[got], len - got, &chunk, NULL) || chunk == 0) return false;
            got += chunk;
        }
        return true;
    }

    static bool DiscordPipe(const wchar_t* pipe_name, std::string& out) {
        HANDLE h = CreateFileW(pipe_name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (h == INVALID_HANDLE_VALUE) return false;

        const std::string hs = "{\"v\":1,\"client_id\":\"839921957679857674\"}";
        const DWORD hs_len = (DWORD)hs.size();
        unsigned char head[8];
        memcpy(head + 0, "\x00\x00\x00\x00", 4);
        memcpy(head + 4, &hs_len, 4);

        DWORD written = 0;
        bool ok = WriteFile(h, head, 8, &written, NULL) &&
            WriteFile(h, hs.data(), (DWORD)hs.size(), &written, NULL);
        if (!ok) { CloseHandle(h); return false; }

        // handshake + folgende frames lesen bis ein user-payload kommt (READY)
        DWORD deadline = GetTickCount() + 5000;
        ok = false;
        while (GetTickCount() < deadline) {
            DWORD op = 0;
            std::string payload;
            if (!ReadFrame(h, op, payload, 1000)) break;
            if (op == 1 && payload.find("\"username\"") != std::string::npos) {
                out = payload;
                ok = true;
                break;
            }
        }

        CloseHandle(h);
        return ok;
    }

    static bool HttpGetBytes(const std::wstring& host, const std::wstring& path, std::vector<unsigned char>& client_out) {
        client_out.clear();
        HINTERNET sess = WinHttpOpen(L"AzureMenu/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!sess) return false;

        HINTERNET conn = WinHttpConnect(sess, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!conn) { WinHttpCloseHandle(sess); return false; }

        HINTERNET req = WinHttpOpenRequest(conn, L"GET", path.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (!req) { WinHttpCloseHandle(conn); WinHttpCloseHandle(sess); return false; }

        bool got = false;
        if (WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
            WinHttpReceiveResponse(req, NULL)) {
            DWORD avail = 0;
            while (WinHttpQueryDataAvailable(req, &avail) && avail > 0) {
                char buf[4096];
                DWORD rd = 0;
                DWORD want = avail < (DWORD)sizeof(buf) ? avail : (DWORD)sizeof(buf);
                if (WinHttpReadData(req, buf, want, &rd) && rd > 0) {
                    client_out.insert(client_out.end(), buf, buf + rd);
                    got = true;
                } else break;
            }
        }

        bool ok = got && !client_out.empty();
        WinHttpCloseHandle(req);
        WinHttpCloseHandle(conn);
        WinHttpCloseHandle(sess);
        return ok;
    }

    static void WorkerFunc() {
        std::string payload;
        bool connected = false;
        for (int i = 0; i < 10; i++) {
            wchar_t pipe_name[64];
            swprintf_s(pipe_name, L"\\\\?\\pipe\\discord-ipc-%d", i);
            if (DiscordPipe(pipe_name, payload)) {
                connected = true;
                break;
            }
        }
        if (!connected || payload.empty()) return;

        const size_t up = payload.find("\"user\":");
        const std::string user = up == std::string::npos ? payload : payload.substr(up + 1);

        std::string id, uname, gname, disc, avatar;
        JsonGetString(user, "id", id);
        JsonGetString(user, "username", uname);
        JsonGetString(user, "global_name", gname);
        JsonGetString(user, "discriminator", disc);
        JsonGetString(user, "avatar", avatar);

        const std::string display = !gname.empty() ? gname : (!uname.empty() ? uname : "Discord");
        {
            std::lock_guard<std::mutex> lock(mutex);
            strncpy_s(name, display.c_str(), sizeof(name) - 1);
            name[sizeof(name) - 1] = '\0';
        }

        std::vector<unsigned char> png;
        if (!id.empty() && !avatar.empty()) {
            const std::wstring path = L"/avatars/" + std::wstring(id.begin(), id.end()) +
                L"/" + std::wstring(avatar.begin(), avatar.end()) + L".png";
            HttpGetBytes(L"cdn.discordapp.com", path, png);
        }
        if (png.empty() && !id.empty()) {
            unsigned long slot = 0;
            for (char c : disc) if (c >= '0' && c <= '9') slot = (slot * 10 + (unsigned long)(c - '0')) % 5;
            const std::wstring path = L"/embed/avatars/" + std::to_wstring(slot) + L".png";
            HttpGetBytes(L"cdn.discordapp.com", path, png);
        }

        {
            std::lock_guard<std::mutex> lock(mutex);
            avatar_png = std::move(png);
        }
        have_account.store(true);
        need_avatar.store(true);
        g_discord_state.have_account = true;
        g_discord_state.need_avatar = true;
    }
}

void discord_refresh_avatar() {
    if (!Discord::need_avatar.load())
        return;
    Discord::need_avatar.store(false);
    g_discord_state.need_avatar = false;

    ID3D11Device* dev = (ID3D11Device*)Menu::g_renderDevice;
    if (!dev) return;

    if (Menu::g_avatarSrv) {
        Menu::g_avatarSrv->Release();
        Menu::g_avatarSrv = nullptr;
    }
    Menu::g_avatarSrv = Discord::CreateAvatarTexture(dev, true);
}

const char* discord_display_name() {
    return Discord::name;
}