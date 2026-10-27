#pragma once
#include <include.h>
#include <src/cache/index.h>

inline uintptr_t Memoryaim = NETCONNECTION + 0x8; //size of NetConnection (0x8)

inline bool memory_event(numerics::fvector newpos)
{
	uintptr_t player_controller = 0;
	{
		std::lock_guard<std::mutex> lock(udata::world_mutex);
		player_controller = udata::world_t.player_controller;
	}
	if (!player_controller || player_controller < 0x10000) {
		return false;
	}

	write<numerics::fvector>(player_controller + Memoryaim, newpos); //write Vectors to control rotation
	return true;
}

inline bool memoryaim_impl(uintptr_t target_mesh, int bone_id)
{
	if (!target_mesh) return false;

	int smoothness = aimbot::smoothness * 5;
	auto snap = bones::get_snapshot(target_mesh);
	numerics::fvector target3d = bones::bone_from_snapshot(snap, bone_id);
	numerics::fvector2d target2d = w2s::project(target3d);

	if (target2d.x == 0 || target2d.y == 0) return false;

	numerics::fvector2d target{};
	target.x = (target2d.x - numerics::screen_width_center) / smoothness;
	target.y = (target2d.y - numerics::screen_height_center) / smoothness;

	numerics::fvector new_rotation = { -target.y / 5, target.x / 5, 0 };
	memory_event(new_rotation);
	return true;
}

// Shared aim state -- updated every frame by esp::draw().
inline bool  g_target_visible   = false;
inline bool  g_aim_active       = false;   // aim and/or trigger bind held
inline void  clear_aim_state()  { g_target_visible = false; }

inline void memoryaim(uintptr_t target_mesh)
{
	memoryaim_impl(target_mesh, 110);
}

// Any aim source (aimbot bind or triggerbot bind) wants a target this frame.
inline bool aim_active()
{
	const bool aim_holds = aimbot::enabled && Menu::IsBindActive({ aimbot::aim_mode, aimbot::aimbot_keybind });
	const bool trigger_holds = triggerbot::enabled && Menu::IsBindActive({ triggerbot::bind_mode, triggerbot::bind });
	return aim_holds || trigger_holds;
}

inline void Aimbot(ImDrawList* dl)
{
	if (!aimbot::enabled && !triggerbot::enabled) return;

	if (aimbot::show_fov)
	{
		const float radius = static_cast<float>(aimbot::fov);
		ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
		if (aimbot::fov_outline)
			dl->AddCircle(center, radius, IM_COL32(0, 0, 0, 230), 0, aimbot::fov_thickness + 1.0f);
		dl->AddCircle(center, radius, ImGui::ColorConvertFloat4ToU32(aimbot::fov_color), 0, aimbot::fov_thickness);
	}

	if (!aim_active()) return;
	if (!udata::closest_mesh) return;

	bool aimbot_holds = aimbot::enabled && Menu::IsBindActive({ aimbot::aim_mode, aimbot::aimbot_keybind });
	int bone_id = aimbot_holds ? aimbot::target_bone : triggerbot::target_bone;

	if (aimbot::visible_only && !g_target_visible)
		return;

	memoryaim_impl(udata::closest_mesh, bone_id);
}

inline void Triggerbot(ImDrawList* dl)
{
	if (!triggerbot::enabled) return;
	if (!triggerbot::show_fov)
	{
		const float radius = static_cast<float>(aimbot::fov);
		ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
		dl->AddCircle(center, radius, ImGui::ColorConvertFloat4ToU32(aimbot::fov_color), 0, aimbot::fov_thickness + 1.0f);
	}
	if (!Menu::IsBindActive({ triggerbot::bind_mode, triggerbot::bind })) return;
	if (!udata::closest_mesh) return;

	if (triggerbot::visible_only && !g_target_visible)
		return;

	static ULONGLONG last_fire = 0;
	const ULONGLONG now = GetTickCount64();
	if (last_fire && now - last_fire < static_cast<ULONGLONG>(triggerbot::cooldown_ms))
		return;

	last_fire = now;
	if (triggerbot::aim_assist)
		memoryaim_impl(udata::closest_mesh, triggerbot::target_bone);

	INPUT down = {};
	down.type = INPUT_MOUSE;
	down.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
	SendInput(1, &down, sizeof(down));

	if (triggerbot::tap_time_ms > 0.5f)
		Sleep(static_cast<DWORD>(triggerbot::tap_time_ms));

	INPUT up = {};
	up.type = INPUT_MOUSE;
	up.mi.dwFlags = MOUSEEVENTF_LEFTUP;
	SendInput(1, &up, sizeof(up));

	last_fire = GetTickCount64();
}