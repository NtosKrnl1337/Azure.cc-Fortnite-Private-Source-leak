#pragma once
#include <atomic>

namespace splash {
inline std::atomic<int> stage{ 0 };  // 0 init, 1 driver, 2 game wait, 3 base, 4 overlay
inline std::atomic<bool> done{ false };
void Start();
void Stop();
} // namespace splash
