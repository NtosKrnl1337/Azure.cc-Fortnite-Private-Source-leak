#pragma once

#include <iostream>
#include <windows.h>
#include <string>   
#include <shlwapi.h>
#include <filesystem>
#include <TlHelp32.h>
#include <chrono>
#include <thread>
#include <cstring>   
#include <utility>   
#include <include.h>

bool IsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;

    if (AllocateAndInitializeSid(&ntAuthority, 2,
        SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0, &adminGroup)) {

        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin;
}

static void SetConsoleFont(const wchar_t* face, SHORT fontW, SHORT fontH, UINT weight = FW_NORMAL)
{
    CONSOLE_FONT_INFOEX cfi{};
    cfi.cbSize = sizeof(cfi);
    cfi.nFont = 0;
    cfi.FontFamily = FF_DONTCARE;
    cfi.FontWeight = weight;
    cfi.dwFontSize.X = fontW;
    cfi.dwFontSize.Y = fontH;
    wcscpy_s(cfi.FaceName, face);
    SetCurrentConsoleFontEx(GetStdHandle(STD_OUTPUT_HANDLE), FALSE, &cfi);
}

static void SetConsoleRowsCols(SHORT cols, SHORT rows)
{
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi{};
    GetConsoleScreenBufferInfo(hOut, &csbi);

    SMALL_RECT win = csbi.srWindow;
    SHORT curWinW = (SHORT)(win.Right - win.Left + 1);
    SHORT curWinH = (SHORT)(win.Bottom - win.Top + 1);

    SMALL_RECT tmpWin = win;
    tmpWin.Right = (SHORT)(tmpWin.Left + min(cols, curWinW) - 1);
    tmpWin.Bottom = (SHORT)(tmpWin.Top + min(rows, curWinH) - 1);
    SetConsoleWindowInfo(hOut, TRUE, &tmpWin);

    COORD bufSize{ cols, rows };
    SetConsoleScreenBufferSize(hOut, bufSize);

    SMALL_RECT newWin{ 0, 0, (SHORT)(cols - 1), (SHORT)(rows - 1) };
    SetConsoleWindowInfo(hOut, TRUE, &newWin);
}

static COORD GetFontCellSizePx()
{
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_FONT_INFO fi{};
    GetCurrentConsoleFont(hOut, FALSE, &fi);
    return fi.dwFontSize;
}

static void FitConsoleToCurrentPixelWindow()
{
    HWND hwnd = GetConsoleWindow();
    RECT rcClient{};
    GetClientRect(hwnd, &rcClient);
    int clientW = rcClient.right - rcClient.left;
    int clientH = rcClient.bottom - rcClient.top;

    COORD cell = GetFontCellSizePx();
    int cols = max(20, clientW / (int)cell.X);
    int rows = max(10, clientH / (int)cell.Y);

    SetConsoleRowsCols((SHORT)cols, (SHORT)rows);
}

static void DisableConsoleResizeAndMaximize()
{
    HWND console = GetConsoleWindow();
    LONG style = GetWindowLong(console, GWL_STYLE);
    style &= ~WS_MAXIMIZEBOX;
    style &= ~WS_SIZEBOX;
    SetWindowLong(console, GWL_STYLE, style);
    SetWindowPos(console, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

static void ResizeConsoleWindowPixels(int widthPx, int heightPx)
{
    HWND console = GetConsoleWindow();
    SetWindowPos(console, nullptr, 0, 0, widthPx, heightPx, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void SetConsoleTransparency(int transparency) {
    HWND hwnd = GetConsoleWindow();
    LONG style = GetWindowLong(hwnd, GWL_EXSTYLE);
    SetWindowLong(hwnd, GWL_EXSTYLE, style | WS_EX_LAYERED);
    SetLayeredWindowAttributes(hwnd, 0, (BYTE)transparency, LWA_ALPHA);
}

void enableANSIColors() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    if (hOut != INVALID_HANDLE_VALUE && GetConsoleMode(hOut, &dwMode)) {
        SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
}

void SetupConsole()
{
    enableANSIColors();
    SetConsoleTransparency(210);

    SetConsoleFont(L"Consolas", 10, 16, FW_NORMAL);
    DisableConsoleResizeAndMaximize();
    ResizeConsoleWindowPixels(600, 400);
    FitConsoleToCurrentPixelWindow();

    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD outMode = 0;
    GetConsoleMode(hOut, &outMode);
    outMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, outMode);

    // Disable text selection (Quick Edit Mode)
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    DWORD inMode = 0;
    GetConsoleMode(hIn, &inMode);

    inMode &= ~ENABLE_QUICK_EDIT_MODE;   // remove Quick Edit
    inMode &= ~ENABLE_MOUSE_INPUT;       // optional: disables mouse input entirely
    inMode |= ENABLE_EXTENDED_FLAGS;    // required when modifying Quick Edit

    SetConsoleMode(hIn, inMode);
}