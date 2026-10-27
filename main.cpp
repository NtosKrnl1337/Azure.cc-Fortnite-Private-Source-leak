#include <include.h>
#include <src/cache/index.h>
#include <src/rendering/index.h>
#include <src/memory/mapper.h>
#include <src/console/console.h>
#include <src/console/log.h>
#include <build.h>
#include <sstream>
#include <src/security/security.hpp>
#include <src/loading/splash.h>

HWND GetProcessWindow(uint32_t pid)
{
    std::pair<HWND, uint32_t> params = { 0, pid };
    BOOL bresult = LI_FN(EnumWindows).cached()([](HWND hwnd, LPARAM lparam) -> BOOL
        {
            auto pparams = (std::pair<HWND, uint32_t>*)(lparam);
            uint32_t processid = 0;
            if (LI_FN(GetWindowThreadProcessId).cached()(hwnd, reinterpret_cast<LPDWORD>(&processid)) && processid == pparams->second)
            {
                LI_FN(SetLastError).cached()((uint32_t)-1);
                pparams->first = hwnd;
                return FALSE;
            }
            return TRUE;
        }, (LPARAM)&params);

    if (!bresult && LI_FN(GetLastError).cached()() == -1 && params.first) return params.first;
    return 0;
}

bool WaitForFortnite(int timeout_seconds = 60)
{
    int elapsed = 0;
    while (elapsed < timeout_seconds)
    {
        DWORD pid = mem::find_process(_xor_(L"FortniteClient-Win64-Shipping.exe"));
        if (pid)
        {
            HWND wnd = GetProcessWindow(pid);
            if (wnd && LI_FN(IsWindowVisible).cached()(wnd))
            {
                RECT rect;
                LI_FN(GetWindowRect).cached()(wnd, &rect);
                int sw = LI_FN(GetSystemMetrics).cached()(SM_CXSCREEN);
                int sh = LI_FN(GetSystemMetrics).cached()(SM_CYSCREEN);
                if (rect.left == 0 && rect.top == 0 && rect.right == sw && rect.bottom == sh)
                {
                    mem::process_id = pid;
                    return true;
                }
            }
        }
        LI_FN(Sleep).cached()(1000);
        elapsed++;
    }
    return false;
}

[[noreturn]] static void SecureExit(int code) {
    LI_FN(Sleep).cached()(static_cast<DWORD>(rand() % 400 + 100));
    LI_FN(TerminateProcess).cached()(LI_FN(GetCurrentProcess).cached()(), static_cast<UINT>(code));
    __assume(false);
}

static void BootLog(const char* stage) {
    char mp[MAX_PATH] = { 0 };
    GetModuleFileNameA(NULL, mp, MAX_PATH);
    std::string lp = mp;
    size_t pp = lp.find_last_of("\\/");
    std::string logp = (pp == std::string::npos ? std::string(".\\") : lp.substr(0, pp + 1)) + "cheat_debug.log";
    FILE* lf = nullptr;
    if (fopen_s(&lf, logp.c_str(), "a") == 0 && lf) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        fprintf(lf, "[%02d:%02d:%02d] %s\n", st.wHour, st.wMinute, st.wSecond, stage);
        fclose(lf);
    }
}

static LONG WINAPI CrashLogger(EXCEPTION_POINTERS* ep) {
    char mp[MAX_PATH] = { 0 };
    GetModuleFileNameA(NULL, mp, MAX_PATH);
    std::string lp = mp;
    size_t pp = lp.find_last_of("\\/");
    std::string logp = (pp == std::string::npos ? std::string(".\\") : lp.substr(0, pp + 1)) + "cheat_debug.log";
    FILE* lf = nullptr;
    if (fopen_s(&lf, logp.c_str(), "a") == 0 && lf) {
        HMODULE self = GetModuleHandleA(NULL);
        uintptr_t rva = (uintptr_t)ep->ExceptionRecord->ExceptionAddress - (uintptr_t)self;
        fprintf(lf, "CRASH code=%08X rva=%llX\n",
            (unsigned)ep->ExceptionRecord->ExceptionCode, (unsigned long long)rva);
        fclose(lf);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

int APIENTRY WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    AddVectoredExceptionHandler(1, CrashLogger);
    BootLog("winmain enter");
    InitSecurity();
    BootLog("security done");

    // No in-cheat key system: the azure.cc loader already validated the license.
    // Styled loading screen only for the "open Fortnite" wait.

    LOG("Product: ", BRIGHT_WHITE);
    LOGRAW("Fortnite Private\n", BRIGHT_BLUE);
    LOG("Version: ", BRIGHT_WHITE);
    LogRaw(LOCAL_VERSION + _xor_("\n\n"), BRIGHT_YELLOW);

    LOG("License validated by loader, initializing...\n", GREEN);
    LOG("Initializing cheat now", WHITE);
    SET_LOG_DELAY(1000);
    LogRaw("...", WHITE);
    SET_LOG_DELAY(10);

    std::cout << std::endl;

    if (!mem::find_driver()) {
        splash::stage.store(1);
        BootLog("mapping driver...");
        bool mapped = MapDriverLocally();
        bool hvci = HvciOn();
        bool admin = IsElevated();
        std::string build = OsBuild();
        char diag[160];
        sprintf_s(diag, "map=%d hvci=%d admin=%d build=%s", mapped ? 1 : 0, hvci ? 1 : 0, admin ? 1 : 0, build.c_str());
        BootLog(diag);
        if (!mapped || !mem::find_driver())
        {
            BootLog("driver failed");
            WebhookReport(_xor_("Driver load failed"), diag);
            if (hvci) {
                LI_FN(MessageBoxA).cached()(nullptr, _xor_("Memory Integrity (HVCI) is ON and blocks the driver.\n\nTurn it OFF:\nWindows Security > Device security > Core isolation details > Memory integrity OFF\n\nThen reboot and try again."), _xor_("Driver blocked"), MB_ICONERROR);
            } else if (!admin) {
                LI_FN(MessageBoxA).cached()(nullptr, _xor_("Start the loader as administrator (right click > run as administrator)."), _xor_("No admin rights"), MB_ICONERROR);
            } else {
                LI_FN(MessageBoxA).cached()(nullptr, _xor_("Failed to load driver. Please check your anti-virus settings, make sure you are on a supported windows version and have fortnite closed."), _xor_("Error"), MB_ICONERROR);
            }
            splash::Stop();
            return 0;
        }
    }

    LOG("Successfully initialized!\n", WHITE);
    LOG("Please open Fortnite!\n", WHITE);
    BootLog("driver ok, waiting fortnite");
    splash::Start();
    splash::stage.store(2);

    if (!WaitForFortnite(120)) {
        BootLog("fortnite timeout");
        WebhookReport(_xor_("Fortnite not found"), _xor_("WaitForFortnite timeout 120s"));
        splash::Stop();
        LI_FN(MessageBoxA).cached()(nullptr, _xor_("Failed to find Fortnite. Make sure it's open and fullscreen."), _xor_("Error"), MB_ICONERROR);
        return 0;
    }

    splash::stage.store(3);

    base = mem::find_image();
    cr3 = mem::fetch_cr3();
    {
        char bb[96];
        sprintf_s(bb, "base=%llX cr3=%llX", (unsigned long long)base, (unsigned long long)cr3);
        BootLog(bb);
    }

    if (!base) {
        WebhookReport(_xor_("Game base failed"), _xor_("find_image null"));
        splash::Stop();
        LI_FN(MessageBoxA).cached()(nullptr, _xor_("Failed to get game base. Please get assistance from your provider by sending a screenshot of this message."), _xor_("Error"), MB_ICONERROR);
        return 0;
    }

    if (!cr3) {
        WebhookReport(_xor_("CR3 failed"), _xor_("fetch_cr3 null"));
        splash::Stop();
        LI_FN(MessageBoxA).cached()(nullptr, _xor_("Failed to get cr3. Please get assistance from your provider by sending a screenshot of this message."), _xor_("Error"), MB_ICONERROR);
        return 0;
    }

    LI_FN(Beep).cached()(750, 300);

    splash::stage.store(4);

    // Config/autosave + Discord worker are owned by the Menu layer now
    // (Menu::Initialize -> ReloadAutosave, Menu::SaveAutosave on exit).
    std::thread(&updater::run).detach();
    BootLog("updater started");
    splash::Stop();
    const bool overlay_started = overlay::run();
    BootLog(overlay_started ? "overlay ok" : "overlay failed");

    if (!overlay_started) {
        WebhookReport(_xor_("Overlay failed"), _xor_("overlay::run false"));
        LI_FN(MessageBoxA).cached()(nullptr, _xor_("Failed to get game window for overlay."), _xor_("Error"), MB_ICONERROR);
        return 0;
    }

    return 0;
}