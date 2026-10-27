#pragma once
#include <include.h>
#include <src/cache/index.h>
#include <src/aim/aimbot.h>
#include <src/rendering/burbank_font.h>
#include <src/rendering/dopamina_font.h>
#include <src/core/Discord.h>
#include <src/security/security.hpp>
#include <d3d11.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <tchar.h>

IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace overlay {

	static constexpr const char* website_url = "https://azurecheats.cc";
	static constexpr const char* discord_url = "https://discord.gg/azuresoftware";

	struct overlay_runtime_state {
		HWND                     hwnd                = nullptr;
        WNDPROC original_wnd_proc = nullptr;
        bool input_attached = false;
		HWND                     game_hwnd           = nullptr;
		HANDLE                   game_process        = nullptr;
		ID3D11Device*            device              = nullptr;
		ID3D11DeviceContext*     ctx                 = nullptr;
		ID3D11RenderTargetView*  rtv                 = nullptr;
		IDXGISwapChain*          swap                = nullptr;
	} static inline overlay_state;

	static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
		if (::ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp)) return 1;
		return DefWindowProc(hwnd, msg, wp, lp);
	}

	static bool get_game_client_rect(RECT& rect) {
		if (!overlay_state.game_hwnd || !IsWindow(overlay_state.game_hwnd)) {
			overlay_state.game_hwnd = FindWindowA(nullptr, "Fortnite  ");
			if (!overlay_state.game_hwnd)
				overlay_state.game_hwnd = FindWindowA(nullptr, "Fortnite");
		}

		if (!overlay_state.game_hwnd || IsIconic(overlay_state.game_hwnd))
			return false;

		RECT client{};
		POINT top_left{};
		if (!GetClientRect(overlay_state.game_hwnd, &client) ||
			!ClientToScreen(overlay_state.game_hwnd, &top_left))
			return false;

		const int width  = client.right - client.left;
		const int height = client.bottom - client.top;
		if (width <= 0 || height <= 0)
			return false;

		rect = { top_left.x, top_left.y, top_left.x + width, top_left.y + height };
		return true;
	}

	static bool position_overlay_over_game() {
		RECT game_rect{};
		if (!get_game_client_rect(game_rect))
			return false;

		const int width  = game_rect.right - game_rect.left;
		const int height = game_rect.bottom - game_rect.top;
		numerics::screen_width         = width;
		numerics::screen_height        = height;
		numerics::screen_width_center  = width / 2;
		numerics::screen_height_center = height / 2;
		numerics::aspect_ratio         = static_cast<float>(width) / static_cast<float>(height);

		static RECT previous_rect{};
		static bool positioned = false;
		if (positioned && EqualRect(&game_rect, &previous_rect))
			return true;

		const bool success = SetWindowPos(overlay_state.hwnd, HWND_TOPMOST,
			game_rect.left, game_rect.top, width, height,
			SWP_NOACTIVATE | SWP_SHOWWINDOW) != FALSE;
		if (success) {
			previous_rect = game_rect;
			positioned = true;
		}
		return success;
	}

	static bool focus_game_window() {
		if (!overlay_state.game_hwnd || !IsWindow(overlay_state.game_hwnd)) {
			overlay_state.game_hwnd = FindWindowA(nullptr, _xor_("Fortnite  "));
			if (!overlay_state.game_hwnd)
				overlay_state.game_hwnd = FindWindowA(nullptr, _xor_("Fortnite"));
		}

		if (!overlay_state.game_hwnd)
			return false;

		if (IsIconic(overlay_state.game_hwnd))
			ShowWindow(overlay_state.game_hwnd, SW_RESTORE);
		else
			ShowWindow(overlay_state.game_hwnd, SW_SHOW);

		const HWND foreground = GetForegroundWindow();
		const DWORD current_thread = GetCurrentThreadId();
		const DWORD game_thread = GetWindowThreadProcessId(overlay_state.game_hwnd, nullptr);
		const DWORD foreground_thread = foreground
			? GetWindowThreadProcessId(foreground, nullptr)
			: 0;

		if (foreground_thread && foreground_thread != current_thread)
			AttachThreadInput(current_thread, foreground_thread, TRUE);
		if (game_thread && game_thread != current_thread)
			AttachThreadInput(current_thread, game_thread, TRUE);

		BringWindowToTop(overlay_state.game_hwnd);
		SetForegroundWindow(overlay_state.game_hwnd);
		SetActiveWindow(overlay_state.game_hwnd);
		SetFocus(overlay_state.game_hwnd);

		if (game_thread && game_thread != current_thread)
			AttachThreadInput(current_thread, game_thread, FALSE);
		if (foreground_thread && foreground_thread != current_thread)
			AttachThreadInput(current_thread, foreground_thread, FALSE);

		return GetForegroundWindow() == overlay_state.game_hwnd;
	}

	static bool is_game_running() {
		if (overlay_state.game_process)
			return WaitForSingleObject(overlay_state.game_process, 0) == WAIT_TIMEOUT;

		return overlay_state.game_hwnd && IsWindow(overlay_state.game_hwnd);
	}

	static bool HijackOverlay() {
		// Own overlay window: no Discord client required. Transparent, topmost,
		// click-through until the menu opens (the render loop toggles interactivity).
		focus_game_window();

		WNDCLASSEXW wc{};
		wc.cbSize = sizeof(wc);
		wc.style = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = wnd_proc;
		wc.hInstance = GetModuleHandleW(nullptr);
		wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
		wc.lpszClassName = L"AzureOverlay";
		RegisterClassExW(&wc);

		RECT r{ 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
		RECT gr{};
		if (get_game_client_rect(gr) && (gr.right - gr.left) > 0 && (gr.bottom - gr.top) > 0)
			r = gr;

		overlay_state.hwnd = CreateWindowExW(
			WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
			L"AzureOverlay", L"",
			WS_POPUP | WS_VISIBLE,
			r.left, r.top, r.right - r.left, r.bottom - r.top,
			nullptr, nullptr, wc.hInstance, nullptr);

		if (!overlay_state.hwnd)
		{
			WebhookReport(_xor_("Overlay failed"), _xor_("CreateWindowExW failed"));
			MessageBoxA(0,
				_xor_("Overlay Failed. Could not create overlay window."),
				_xor_("Overlay Error"),
				MB_ICONINFORMATION);
			return false;
		}

		MARGINS Margin = { -1, -1, -1, -1 };
		DwmExtendFrameIntoClientArea(overlay_state.hwnd, &Margin);

		if (!position_overlay_over_game())
		{
			WebhookReport(_xor_("Overlay failed"), _xor_("position_overlay_over_game failed"));
			return false;
		}
		if (!SetLayeredWindowAttributes(overlay_state.hwnd, RGB(0, 0, 0), 255, LWA_ALPHA))
		{
			WebhookReport(_xor_("Overlay failed"), _xor_("SetLayeredWindowAttributes failed"));
			return false;
		}

		return true;
	}

	static bool create_d3d() {
		DXGI_SWAP_CHAIN_DESC desc{};
		desc.BufferCount        = 2;
		desc.BufferDesc.Format  = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.BufferDesc.RefreshRate.Numerator   = 60;
		desc.BufferDesc.RefreshRate.Denominator = 1;
		desc.Flags              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		desc.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		desc.OutputWindow       = overlay_state.hwnd;
		desc.SampleDesc.Count   = 1;
		desc.Windowed           = TRUE;
		desc.SwapEffect         = DXGI_SWAP_EFFECT_DISCARD;

		const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
		D3D_FEATURE_LEVEL got;

		if (FAILED(D3D11CreateDeviceAndSwapChain(
			nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
			levels, _countof(levels),
			D3D11_SDK_VERSION, &desc,
			&overlay_state.swap, &overlay_state.device, &got, &overlay_state.ctx)))
			return false;

		ID3D11Texture2D* back = nullptr;
		if (FAILED(overlay_state.swap->GetBuffer(0, IID_PPV_ARGS(&back))) || !back) return false;
		if (FAILED(overlay_state.device->CreateRenderTargetView(back, nullptr, &overlay_state.rtv))) return false;
		back->Release();

		ImGui::CreateContext();
		ImGui::StyleColorsDark();

		auto& io = ImGui::GetIO();
		io.IniFilename = nullptr;
		g_font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 15, nullptr);
		if (!g_font) g_font = io.Fonts->AddFontDefault();
		io.FontDefault = g_font;

		// Menu::Initialize rebuilds the font atlas; register everything that must
		// outlive it (watermark + ESP) so their ImFont pointers stay valid.
		ImFontConfig menu_font_config{};
		menu_font_config.FontDataOwnedByAtlas = false;
		g_watermark_font = io.Fonts->AddFontFromMemoryTTF(
			dopamina_font_data, sizeof(dopamina_font_data), 14.5f, &menu_font_config);
		if (!g_watermark_font) g_watermark_font = g_font;
		ImFontConfig esp_font_config{};
		esp_font_config.FontDataOwnedByAtlas = false;
		g_esp_font = io.Fonts->AddFontFromMemoryTTF(
			g_burbank_font_bytes,
			static_cast<int>(sizeof(g_burbank_font_bytes)),
			30.0f,
			&esp_font_config);
		if (!g_esp_font) g_esp_font = g_font;
		Menu::SetEspFont(g_esp_font);

		ImGui_ImplWin32_Init(overlay_state.hwnd);
		ImGui_ImplDX11_Init(overlay_state.device, overlay_state.ctx);
		Menu::SetDevice(overlay_state.device);
		Menu::Initialize();
		overlay_state.device->Release();
		Sleep(100);
		return true;
	}

	static void open_url(const char* url) {
		ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL);
	}

	static bool menu() {
		return Menu::Render();
	}

    static void release_overlay_input() {
        if (!overlay_state.input_attached) return;
        overlay_state.input_attached = false;
        if (!overlay_state.hwnd || !IsWindow(overlay_state.hwnd)) return;

        const bool had_focus = GetForegroundWindow() == overlay_state.hwnd;
        if (GetCapture() == overlay_state.hwnd) ReleaseCapture();
        const LONG_PTR style = GetWindowLongPtr(overlay_state.hwnd, GWL_EXSTYLE);
        SetWindowLongPtr(overlay_state.hwnd, GWL_EXSTYLE, style | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
        SetWindowPos(overlay_state.hwnd, nullptr, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);

        // Remove our callback before destroying its ImGui context.
        if (overlay_state.original_wnd_proc &&
            GetWindowLongPtr(overlay_state.hwnd, GWLP_WNDPROC) == reinterpret_cast<LONG_PTR>(wnd_proc)) {
            SetWindowLongPtr(overlay_state.hwnd, GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(overlay_state.original_wnd_proc));
        }
        overlay_state.original_wnd_proc = nullptr;
        if (had_focus && overlay_state.game_hwnd && IsWindow(overlay_state.game_hwnd))
            SetForegroundWindow(overlay_state.game_hwnd);
    }

	static void loop() {
		constexpr float clear_color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		MSG msg{};
		ULONGLONG next_game_check = 0;

		while (msg.message != WM_QUIT && !io_state::exit_requested && !io_state::exit_requested) {
			while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}

			auto& io = ImGui::GetIO();
			io.DeltaTime = 1.0f / 60.0f;

			const ULONGLONG now = GetTickCount64();
			if (now >= next_game_check) {
				next_game_check = now + 1000;
				if (!is_game_running()) {
					io_state::exit_requested = true;
					break;
				}
			}

			// Bind-state handling (menu toggle + exit key) lives inside Menu::Render,
			// which runs the sync that also refreshes io_state::menu_open.

			position_overlay_over_game();

			{
				static int lw = 0, lh = 0;
				RECT r;
				if (GetClientRect(overlay_state.hwnd, &r)) {
					int cw = r.right - r.left;
					int ch = r.bottom - r.top;
					if (cw > 0 && ch > 0 && (cw != lw || ch != lh)) {
						lw = cw; lh = ch;
						overlay_state.ctx->OMSetRenderTargets(0, nullptr, nullptr);
						if (overlay_state.rtv) { overlay_state.rtv->Release(); overlay_state.rtv = nullptr; }
						overlay_state.swap->ResizeBuffers(0, cw, ch, DXGI_FORMAT_UNKNOWN, 0);
						ID3D11Texture2D* back = nullptr;
						if (SUCCEEDED(overlay_state.swap->GetBuffer(0, IID_PPV_ARGS(&back))) && back) {
							overlay_state.device->CreateRenderTargetView(back, nullptr, &overlay_state.rtv);
							back->Release();
						}
					}
				}
			}

            // Keep the overlay style/focus stable while the menu is up.
            const bool overlay_interactive = io_state::menu_open;
            static bool was_open = overlay_interactive;
			static HWND prev_fg  = nullptr;
			if (was_open != overlay_interactive) {
				was_open = overlay_interactive;
				LONG ex = GetWindowLong(overlay_state.hwnd, GWL_EXSTYLE);
				if (overlay_interactive) ex &= ~(WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
				else           ex |=  (WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
				SetWindowLong(overlay_state.hwnd, GWL_EXSTYLE, ex);
				SetWindowPos(overlay_state.hwnd, nullptr, 0, 0, 0, 0,
					SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_SHOWWINDOW);

				if (overlay_interactive) {
					HWND fg = GetForegroundWindow();
					if (fg && fg != overlay_state.hwnd) prev_fg = fg;
					SetForegroundWindow(overlay_state.hwnd);
					SetFocus(overlay_state.hwnd);
				} else if (prev_fg && IsWindow(prev_fg)) {
					SetForegroundWindow(prev_fg);
				}
			}

			ImGui_ImplDX11_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			esp::draw(ImGui::GetBackgroundDrawList());
			Aimbot(ImGui::GetBackgroundDrawList());
			menu();

			const bool menu_open = io_state::menu_open; // refreshed by Menu::sync inside Render
			if (menu_open) {
				POINT p;
				GetCursorPos(&p);
				ScreenToClient(overlay_state.hwnd, &p);
				io.MousePos = ImVec2((float)p.x, (float)p.y);
				io.MouseDown[0] = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
			}

			ImGui::Render();
			discord_refresh_avatar();
			overlay_state.ctx->OMSetRenderTargets(1, &overlay_state.rtv, nullptr);
			overlay_state.ctx->ClearRenderTargetView(overlay_state.rtv, clear_color);
			ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
			overlay_state.swap->Present(1, 0);
		}

        release_overlay_input();

        // Clear the last overlay frame before the normal shutdown/config-save path.
        if (overlay_state.rtv) {
            overlay_state.ctx->ClearRenderTargetView(overlay_state.rtv, clear_color);
            overlay_state.swap->Present(1, 0);
        }
        discord_refresh_avatar();
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		Menu::ShutdownUi();
		ImGui::DestroyContext();

		if (overlay_state.game_process) {
			CloseHandle(overlay_state.game_process);
			overlay_state.game_process = nullptr;
		}

		Menu::SaveAutosave();
	}

	static bool run() {
		if (!HijackOverlay()) {
            release_overlay_input();
			return false; 
		}

		if (!create_d3d()) {
            release_overlay_input();
			MessageBoxA(nullptr, _xor_("Failed to initialize Direct3D."), _xor_("Error"), MB_ICONERROR | MB_OK);
			return false;
		}

		overlay_state.game_process = OpenProcess(SYNCHRONIZE, FALSE, mem::process_id);

		loop();

		return true;
	}

}
