#pragma once

#include <chrono>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <src/security/xor.h>

#define BLACK          "\033[30m"
#define RED            "\033[31m"
#define GREEN          "\033[32m"
#define YELLOW         "\033[33m"
#define BLUE           "\033[34m"
#define MAGENTA        "\033[35m"
#define CYAN           "\033[36m"
#define WHITE          "\033[37m"
#define BRIGHT_BLACK   "\033[90m"
#define BRIGHT_RED     "\033[91m"
#define BRIGHT_GREEN   "\033[92m"
#define BRIGHT_YELLOW  "\033[93m"
#define BRIGHT_BLUE    "\033[94m"
#define BRIGHT_MAGENTA "\033[95m"
#define BRIGHT_CYAN    "\033[96m"
#define BRIGHT_WHITE   "\033[97m"
#define RESET          "\033[0m"

namespace LogConfig {
    inline int g_LogDelayMs = 10;
}

#define SET_LOG_DELAY(ms) (LogConfig::g_LogDelayMs = (ms))
#define GET_LOG_DELAY()   (LogConfig::g_LogDelayMs)

inline void PrintTypewriter(std::string_view message)
{
    const int delay = LogConfig::g_LogDelayMs;
    for (char ch : message) {
        std::cout << ch << std::flush;
        const int charDelay = (ch == ' ') ? (delay / 2) : delay;
        if (charDelay > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(charDelay));
        }
    }
}

inline void Log(std::string_view message, const std::string& color)
{
    std::cout << WHITE;
    PrintTypewriter("[");
    std::cout << BRIGHT_CYAN;
    PrintTypewriter("*");
    std::cout << WHITE;
    PrintTypewriter("] ");

    std::cout << color;
    PrintTypewriter(message);
    std::cout << RESET;
}

inline void LogRaw(std::string_view message, const std::string& color)
{
    std::cout << color;
    PrintTypewriter(message);
    std::cout << RESET;
}

#define LOG(msg, color)    Log(_xor_(msg), color)
#define LOGRAW(msg, color) LogRaw(_xor_(msg), color)