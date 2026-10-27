#pragma once
#include <src/rendering/menu_cs.h>
#include <thirdparty/imgui/imgui.h>
#include <thirdparty/imgui/imgui_impl_win32.h>
#include <thirdparty/imgui/imgui_impl_dx11.h>
#include <thirdparty/imgui/imgui_internal.h>
#include <src/engine/sdk.h>

inline ImFont* g_font = nullptr;
inline ImFont* g_watermark_font = nullptr;
inline ImFont* g_esp_font = nullptr;

namespace updater {
	void run();
}

namespace esp {
    inline constexpr float fov_triangle_thickness = 1.0f;
    inline bool flags_enabled = true;
    inline bool fov_triangle_filled = true;
    inline bool fov_triangle_outline = false;
    inline bool fov_triangle_dynamic = true;

	void draw(ImDrawList* dl);

	inline bool  enabled       = false;
    inline bool  team_enabled  = false;
	inline bool  box           = false;
	inline bool  team_box      = false;
    inline bool text_outline = true;
    inline bool box_outline = false;
    inline bool corner_box_outline = false;
    inline bool skeleton_outline = false;
    inline bool tracers_outline = false;

	inline bool  skeleton      = false;
	inline bool  team_skeleton = false;
	inline bool  tracers       = false;
	inline bool  team_tracer   = false;
	inline bool  show_name     = true;
	inline bool  team_name     = true;
	inline bool  show_distance = true;
	inline bool  show_platform = false;
	inline bool  show_rank     = false;
	inline bool  show_weapon   = false;
	inline bool  team_weapon_text = false;
	inline bool  show_weapon_icon = false;
	inline bool  team_weapon_icon = false;
	inline bool  show_ammo     = false;
	inline bool  fov_triangles = false;
	inline bool  show_health   = false;
	inline bool  team_health   = false;
	inline bool  show_hp_text  = false;
	inline bool  team_hp_text  = false;
	inline bool  watermark_enabled = true;

	inline float box_thickness = 1.0f;
    inline float corner_box_thickness = 1.0f;
    inline float skeleton_thickness = 1.0f;
    inline float tracers_thickness = 1.0f;
    inline int box_type = 0;
	inline constexpr float max_distance  = 300.0f;
	inline float fov_triangle_size   = 12.0f;
	inline float fov_triangle_offset = 18.0f;
	inline float fov_triangle_width  = 2.4f;
	inline float text_size           = 15.0f;

    // Palette synced from the menu config each frame by Menu::sync().
    inline ImU32 enemy_color         = IM_COL32(255, 255, 255, 255);
    inline ImU32 enemy_tracer_color  = IM_COL32(255, 255, 255, 255);
    inline ImU32 enemy_skeleton_color = IM_COL32(255, 255, 255, 255);
    inline ImU32 enemy_name_color    = IM_COL32(255, 255, 255, 255);
    inline ImU32 enemy_health_color  = IM_COL32(120, 220, 120, 255);

    inline ImU32 team_color          = IM_COL32(80, 160, 250, 255);
    inline ImU32 team_tracer_color   = IM_COL32(80, 160, 250, 255);
    inline ImU32 team_skeleton_color = IM_COL32(140, 220, 255, 255);
    inline ImU32 team_name_color     = IM_COL32(80, 160, 250, 255);
    inline ImU32 team_health_color   = IM_COL32(120, 220, 120, 255);

    struct feature_colors {
        ImVec4 visible = ImVec4(0.35f, 0.75f, 1.0f, 1.0f);
        ImVec4 hidden = ImVec4(1, 1, 1, 1);
        ImU32 get(bool is_visible) const {
            return ImGui::ColorConvertFloat4ToU32(is_visible ? visible : hidden);
        }
    };
    inline feature_colors box_colors;
    inline feature_colors corner_box_colors;
    inline feature_colors skeleton_colors;
    inline feature_colors tracers_colors;
    inline feature_colors flags_colors;
    inline feature_colors arrows_colors;

}

namespace aimbot {
    inline ImVec4 fov_color = ImVec4(1, 1, 1, 1);
    inline float fov_thickness = 1.0f;
    inline bool fov_outline = false;
	inline bool enabled = false;
	inline bool show_fov = true;
	inline int smoothness = 5;
	inline int fov = 100;
	inline int aimbot_keybind = VK_RBUTTON;
    inline int aim_mode = 1;
    inline int target_bone = 0;
    inline bool visible_only = true;
}

namespace triggerbot {
    inline bool enabled = false;
    inline int bind = VK_RBUTTON;
    inline int bind_mode = 1;
    inline bool visible_only = true;
    inline bool aim_assist = true;
    inline int target_bone = 0;
    inline float tap_time_ms = 1.0f;
    inline float cooldown_ms = 90.0f;
    inline bool show_fov = true;
}

namespace rage {
    inline bool walls = false;
    inline bool speed = false;
    inline float speedVal = 1.5f;
    inline bool infAmmo = false;
    inline bool noReload = false;
    inline bool rapidFire = false;
    inline bool fovOn = false;
    inline float fovVal = 110.0f;
    inline bool spinbot = false;
    inline int zoomKey = 0;
    inline float zoomFov = 30.0f;
    inline int panicKey = 0;
    void apply();
}

namespace io_state {
    inline int menu_key = VK_INSERT;
    inline int exit_key = VK_ESCAPE;
	inline bool menu_open = false;
	inline bool exit_requested = false;
}

namespace bridge {
    inline Menu::Config* cfg = &Menu::GetConfig();
    inline void sync() { Menu::sync(); }
}