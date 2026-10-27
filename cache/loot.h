#pragma once
// loot.h — container / pickup / supply-drop ESP backend.
// GObjects scan in a background thread (called from updater), guarded reads
// only: worst case is an empty list, never a crash.
#include <src/engine/sdk.h>
#include <src/memory/driver.h>

#include <vector>
#include <mutex>
#include <string>
#include <cctype>
#include <cmath>
#include <cstdio>

namespace loot {

// GObjects / GNames RVAs (per game dump)
inline constexpr std::uintptr_t kGObjects = 0x1B1D3640;
inline constexpr std::uintptr_t kGNames = 0x1B179F00;
inline constexpr int32_t kChunkOff = 0x60;
inline constexpr int32_t kNumOff = 0x5C;
inline constexpr int32_t kItemSize = 0x18;
inline constexpr int32_t kPerChunk = 0x10000;
inline constexpr int32_t kObjClassOff = 0x20;
inline constexpr int32_t kObjNameOff = 0x18;
inline constexpr int32_t kRootCompOff = 0x1B0;
inline constexpr int32_t kComp2WorldOff = 0x1E0;
inline constexpr int32_t kTransOff = 0x10;

struct entry {
    double x = 0, y = 0, z = 0;
    int kind = 0; // 0 chest, 1 pickup, 2 drop
    int tier = 0;
};

inline std::vector<entry> items;
inline std::mutex mtx;
inline bool wantChests = false;
inline bool wantPickups = false;
inline bool wantDrops = false;
inline bool showDist = true;
inline float maxDist = 150.0f;
inline ImU32 chestColor = IM_COL32(255, 210, 90, 255);

inline std::string FNameText(uint32_t id) {
    uint32_t block = id >> 16;
    uint32_t offset = id & 0xFFFF;
    if (offset + 2 > 0x2000) return "";
    uintptr_t pool = ::base + kGNames;
    if (!Valid(pool)) return "";
    int32_t count = read<int32_t>(pool + 0x18);
    if (count <= 0 || block >= (uint32_t)count) return "";
    uintptr_t blocks = read<uintptr_t>(pool + 0x10);
    if (!Valid(blocks)) return "";
    uintptr_t bbase = read<uintptr_t>(blocks + (uint64_t)block * 8);
    if (!Valid(bbase)) return "";
    uint16_t header = read<uint16_t>(bbase + offset);
    bool wide = (header & 1) != 0;
    int len = (header >> 6) & 0x3FF;
    if (len <= 0 || len > 200) return "";
    size_t need = 2 + (size_t)len * (wide ? 2 : 1);
    if (offset + need > 0x2000) return "";
    std::string o;
    o.reserve((size_t)len);
    if (wide) {
        for (int i = 0; i < len; ++i) {
            uint16_t c = read<uint16_t>(bbase + offset + 2 + (size_t)i * 2);
            o += (c && c < 128) ? (char)c : '?';
        }
    } else {
        for (int i = 0; i < len; ++i) {
            char c = read<char>(bbase + offset + 2 + (size_t)i);
            o += (c >= 32 && c < 127) ? c : '?';
        }
    }
    for (char& c : o) c = (char)std::tolower((unsigned char)c);
    return o;
}

inline int MatchKind(const std::string& n) {
    if (n.find("buildingcontainer") != std::string::npos) return 0;
    if (n == "fortpickup") return 1;
    if (n.find("supplydrop") != std::string::npos) return 2;
    return -1;
}

inline void refresh(uintptr_t modBase, numerics::fvector localPos) {
    std::vector<entry> found;
    found.reserve(256);
    if (!Valid(modBase)) return;
    uintptr_t arr = modBase + kGObjects;
    int32_t num = read<int32_t>(arr + kNumOff);
    if (num <= 0 || num > 600000) return;
    uintptr_t chunks = read<uintptr_t>(arr + kChunkOff);
    if (!Valid(chunks)) return;
    for (int32_t idx = 0; idx < num && (int)found.size() < 400; ++idx) {
        int32_t ci = idx / kPerChunk;
        int32_t wi = idx % kPerChunk;
        uintptr_t chunk = read<uintptr_t>(chunks + (uint64_t)ci * 8);
        if (!Valid(chunk)) continue;
        uintptr_t obj = read<uintptr_t>(chunk + (uint64_t)wi * kItemSize);
        if (!Valid(obj)) continue;
        uintptr_t cls = read<uintptr_t>(obj + kObjClassOff);
        if (!Valid(cls)) continue;
        uint32_t nid = read<uint32_t>(cls + kObjNameOff);
        if (!nid) continue;
        std::string n = FNameText(nid);
        if (n.empty()) continue;
        int kind = MatchKind(n);
        if (kind < 0) continue;
        if (kind == 0 && !wantChests) continue;
        if (kind == 1 && !wantPickups) continue;
        if (kind == 2 && !wantDrops) continue;
        uintptr_t root = read<uintptr_t>(obj + kRootCompOff);
        if (!Valid(root)) continue;
        numerics::fvector pos = read<numerics::fvector>(root + kComp2WorldOff + kTransOff);
        double dx = pos.x - localPos.x, dy = pos.y - localPos.y, dz = pos.z - localPos.z;
        double dist = std::sqrt(dx * dx + dy * dy + dz * dz) * 0.01;
        if (dist > maxDist) continue;
        int tier = 0;
        if (kind == 1) {
            uintptr_t e = read<uintptr_t>(obj + 0x368);
            if (Valid(e)) {
                uintptr_t def = read<uintptr_t>(e + 0x0C);
                if (Valid(def)) {
                    int t = (int)read<uint8_t>(def + 0x74);
                    if (t >= 1 && t <= 30) tier = t;
                }
            }
        }
        entry en;
        en.x = pos.x; en.y = pos.y; en.z = pos.z;
        en.kind = kind; en.tier = tier;
        found.push_back(en);
    }
    {
        std::lock_guard<std::mutex> lock(mtx);
        items = std::move(found);
    }
}

inline ImU32 TierColor(int tier) {
    if (tier <= 1) return IM_COL32(160, 160, 160, 255);
    if (tier == 2) return IM_COL32(80, 200, 90, 255);
    if (tier == 3) return IM_COL32(80, 150, 250, 255);
    if (tier == 4) return IM_COL32(170, 90, 250, 255);
    return IM_COL32(250, 160, 50, 255);
}

inline void draw(ImDrawList* dl, bool showDist) {
    std::vector<entry> cur;
    {
        std::lock_guard<std::mutex> lock(mtx);
        cur = items;
    }
    if (cur.empty()) return;
    int drawn = 0;
    for (auto& e : cur) {
        if (drawn >= 120) break;
        numerics::fvector2d s = w2s::project(numerics::fvector(e.x, e.y, e.z));
        if (s.x == 0 && s.y == 0) continue;
        if (!w2s::on_screen(s)) continue;
        const char* label = e.kind == 0 ? "Chest" : (e.kind == 1 ? "Loot" : "Supply Drop");
        ImU32 col = e.kind == 0 ? chestColor : (e.kind == 1 ? TierColor(e.tier) : IM_COL32(250, 160, 50, 255));
        float fx = (float)s.x, fy = (float)s.y;
        if (e.kind == 0) {
            dl->AddRect(ImVec2(fx - 9, fy - 7), ImVec2(fx + 9, fy + 7), col, 2.0f, 0, 1.5f);
        } else {
            dl->AddQuadFilled(ImVec2(fx, fy - 6), ImVec2(fx + 6, fy), ImVec2(fx, fy + 6), ImVec2(fx - 6, fy), col);
        }
        char txt[64];
        if (showDist) {
            double dx = e.x - camera::location.x, dy = e.y - camera::location.y, dz = e.z - camera::location.z;
            int m = (int)(std::sqrt(dx * dx + dy * dy + dz * dz) * 0.01);
            sprintf_s(txt, "%s [%dm]", label, m);
        } else {
            sprintf_s(txt, "%s", label);
        }
        ImVec2 tsz = ImGui::CalcTextSize(txt);
        dl->AddText(ImVec2(fx - tsz.x * 0.5f, fy + 9), col, txt);
        ++drawn;
    }
}

} // namespace loot
