#include "pch.h"
#include "Menu.hpp"

#include <algorithm>
#include <sstream>
#include <iomanip>

#include "dependency/imgui/imgui.h"
#include "dependency/imgui/imgui_internal.h"
#include "dependency/imgui/backend/imgui_impl_dx9.h"
#include "dependency/imgui/backend/imgui_impl_win32.h"

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace
{
	void applyRoundedWindowRegion(HWND hWnd)
	{
		RECT clientRect{};
		if (!GetClientRect(hWnd, &clientRect))
			return;

		const int width = clientRect.right - clientRect.left;
		const int height = clientRect.bottom - clientRect.top;
		if (width <= 0 || height <= 0)
			return;

		const int radius = std::min(30, std::min(width, height) / 8);
		HRGN roundedRegion = CreateRoundRectRgn(0, 0, width + 1, height + 1, radius * 2, radius * 2);
		if (roundedRegion != nullptr && !SetWindowRgn(hWnd, roundedRegion, TRUE))
			DeleteObject(roundedRegion);
	}

	ImVec4 canvasColor(bool darkTheme)
	{
		return darkTheme
			? ImVec4(0.035f, 0.047f, 0.078f, 1.0f)
			: ImVec4(0.955f, 0.970f, 0.990f, 1.0f);
	}

	ImVec4 outerCanvasColor(bool darkTheme)
	{
		return darkTheme
			? ImVec4(0.075f, 0.095f, 0.140f, 1.0f)
			: ImVec4(0.875f, 0.910f, 0.960f, 1.0f);
	}
}

bool Menu::initialize()
{
	// Create application window
	//ImGui_ImplWin32_EnableDpiAwareness();
	WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, Menu::WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("WC"), NULL };
	::RegisterClassEx(&wc);
	constexpr auto windowTitle = L"Potato Injector";
	this->hwnd = ::CreateWindow(wc.lpszClassName, windowTitle,
		WS_POPUP,
		100, 100, 500, 640, NULL, NULL, wc.hInstance, NULL);
	if (this->hwnd == nullptr)
	{
		::UnregisterClass(wc.lpszClassName, wc.hInstance);
		return false;
	}
	applyRoundedWindowRegion(this->hwnd);

	// Initialize Direct3D
	if (!createD3D9Device(hwnd))
	{
		cleanupD3D9Device();
		::UnregisterClass(wc.lpszClassName, wc.hInstance);
		return 1;
	}

	// Show the window
	::ShowWindow(hwnd, SW_SHOWDEFAULT);
	::UpdateWindow(hwnd);

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.WantSaveIniSettings = false;

	// Setup Dear ImGui style
	setupMenuStyle(true, 1);

	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX9_Init(this->d3dDevice);

	this->isMenuOn = true;
	std::thread(&Menu::detectGame, this).detach();
	std::thread(&Menu::detectSteam, this).detach();
	std::thread(&Menu::updateFiles, this).detach();

	return true;
}

void Menu::loop()
{
	// Main loop
	while (this->isMenuOn)
	{
		MSG msg;
		while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
			if (msg.message == WM_QUIT)
				this->isMenuOn = false;
		}
		if (!this->isMenuOn)
			break;

		// Start the Dear ImGui frame
		ImGui_ImplDX9_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
		constexpr float outerMargin = 6.0f;
		ImGui::SetNextWindowPos(ImVec2(outerMargin, outerMargin), ImGuiCond_Always);
		ImGui::SetNextWindowSize(
			ImVec2(std::max(0.0f, displaySize.x - outerMargin * 2.0f),
				std::max(0.0f, displaySize.y - outerMargin * 2.0f)),
			ImGuiCond_Always);
		ImGui::Begin("Potato Injector", nullptr,
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoScrollbar);
		ImGui::BeginChild("Hero", ImVec2(0, 86), true);
		ImGui::BeginGroup();
		ImGui::TextColored(isDarkTheme ? ImVec4(0.42f, 0.80f, 1.00f, 1.00f) : ImVec4(0.08f, 0.40f, 0.78f, 1.00f), "POTATO INJECTOR");
		ImGui::TextDisabled("A clean workspace for your selected module");
		ImGui::EndGroup();
		ImGui::SameLine(ImGui::GetWindowWidth() - 125.0f);
		if (ImGui::Button(isDarkTheme ? "Day mode" : "Night mode", ImVec2(104.0f, 30.0f)))
		{
			isDarkTheme = !isDarkTheme;
			setupMenuStyle(isDarkTheme, 1.0f);
		}
		ImGui::SameLine(0.0f, 8.0f);
		if (ImGui::Button("X", ImVec2(28.0f, 30.0f)))
			::PostMessage(hwnd, WM_CLOSE, 0, 0);
		ImGui::TextDisabled("%s theme  -  live monitoring enabled", isDarkTheme ? "Night" : "Day");
		ImGui::EndChild();
		ImGui::Spacing();

		renderStatusPanel();
		renderTargetPanel();
		const auto paths = snapshotDllPaths();
		renderInjectionPanel(paths);

		ImGui::End();

		// Rendering
		ImGui::EndFrame();

		this->d3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
		this->d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		this->d3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
		const ImVec4 clearColor = outerCanvasColor(this->isDarkTheme);
		D3DCOLOR clear_col_dx = D3DCOLOR_RGBA((int)(clearColor.x * 255.0f), (int)(clearColor.y * 255.0f), (int)(clearColor.z * 255.0f), 255);
		this->d3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clear_col_dx, 1.0f, 0);
		if (this->d3dDevice->BeginScene() >= 0)
		{
			ImGui::Render();
			ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
			this->d3dDevice->EndScene();
		}
		HRESULT result = this->d3dDevice->Present(NULL, NULL, NULL, NULL);

	}
}

bool Menu::createD3D9Device(HWND hWnd)
{
	if ((this->pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == NULL) return false;

	ZeroMemory(&this->d3dpp, sizeof(this->d3dpp));
	this->d3dpp.Windowed = TRUE;
	this->d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	this->d3dpp.BackBufferFormat = D3DFMT_UNKNOWN; 
	this->d3dpp.EnableAutoDepthStencil = TRUE;
	this->d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
	this->d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;       
	this->d3dpp.hDeviceWindow = hWnd;
	auto result = this->pD3D->CreateDevice(
		D3DADAPTER_DEFAULT,
		D3DDEVTYPE_HAL,
		hwnd,
		D3DCREATE_HARDWARE_VERTEXPROCESSING,
		&this->d3dpp, &this->d3dDevice);
	if (result != S_OK) return false;

	return true;
}

void Menu::cleanupD3D9Device()
{
	if (this->d3dDevice != nullptr)
	{
		this->d3dDevice->Release();
		this->d3dDevice = nullptr;
	}

	if (this->pD3D != nullptr)
	{
		this->pD3D->Release();
		this->pD3D = nullptr;
	}
}

LRESULT __stdcall Menu::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return true;

	switch (msg)
	{
	case WM_NCHITTEST:
	{
		POINT cursor{
			static_cast<LONG>(static_cast<short>(LOWORD(lParam))),
			static_cast<LONG>(static_cast<short>(HIWORD(lParam))) };
		::ScreenToClient(hWnd, &cursor);
		// Keep the Hero area draggable while leaving the theme and close buttons clickable.
		if (cursor.y >= 0 && cursor.y < 86 && cursor.x < 330)
			return HTCAPTION;
		break;
	}
	case WM_SIZE:
		applyRoundedWindowRegion(hWnd);
		break;
	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
		break;
	case WM_DESTROY:
		::PostQuitMessage(0);
		return 0;
	}
	return ::DefWindowProc(hWnd, msg, wParam, lParam);
}

void Menu::renderStatusPanel()
{
	ImGui::BeginChild("StatusPanel", ImVec2(0, 112), true);
	ImGui::TextDisabled("SYSTEM STATUS");
	ImGui::SameLine(ImGui::GetWindowWidth() - 96.0f);
		ImGui::TextColored(ImVec4(0.35f, 0.82f, 0.55f, 1.0f), "ONLINE");
	ImGui::Separator();

	const auto status = [](const char* label, bool active, const char* activeText, const char* inactiveText)
	{
		ImGui::TextColored(active ? ImVec4(0.35f, 0.85f, 0.55f, 1.0f) : ImVec4(0.92f, 0.38f, 0.42f, 1.0f), ">>");
		ImGui::SameLine();
		ImGui::TextUnformatted(label);
		ImGui::SameLine(178.0f);
		ImGui::TextColored(active ? ImVec4(0.35f, 0.85f, 0.55f, 1.0f) : ImVec4(0.92f, 0.38f, 0.42f, 1.0f), "%s", active ? activeText : inactiveText);
	};

	status("Steam", g_injector->steamRunning, "RUNNING", "OFFLINE");
	status("CS2", g_injector->csgoRunning, this->isInjecting ? "INJECTING" : "RUNNING", "OFFLINE");
	ImGui::TextDisabled("VAC3 patching is disabled");
	ImGui::EndChild();
}

void Menu::renderTargetPanel()
{
	ImGui::BeginChild("TargetPanel", ImVec2(0, 150), true);
		ImGui::TextDisabled("TARGET PROCESS");
		ImGui::Separator();
		ImGui::Checkbox("Auto-close after operation", &g_injector->shouldAutoExit);
		ImGui::Checkbox("Use custom process", &g_injector->isCustomProcess);

		if (g_injector->isCustomProcess)
		{
			auto processes = mem::getProcList();
			std::string processItems;
			std::vector<std::wstring> processNames;
			for (const auto& process : processes)
			{
				for (const auto character : process.second)
					processItems += static_cast<char>(character);
				processItems.push_back('\0');
				processNames.push_back(process.second);
			}
			processItems.push_back('\0');

			if (!processNames.empty())
			{
				this->selectedProcess = std::clamp(this->selectedProcess, 0, static_cast<int>(processNames.size()) - 1);
				if (ImGui::Combo("Process", &this->selectedProcess, processItems.c_str()))
					g_injector->customProcessName = processNames[this->selectedProcess];
			}
			else
			{
				ImGui::TextDisabled("No running processes found");
			}
		}
		else
		{
			ImGui::TextDisabled("Target: Counter-Strike 2");
		}
	ImGui::EndChild();
}

std::vector<std::string> Menu::snapshotDllPaths()
{
	std::scoped_lock lock(this->mtx);
	return this->filePaths;
}

void Menu::renderInjectionPanel(const std::vector<std::string>& paths)
{
	ImGui::BeginChild("ModulePanel", ImVec2(0, 0), true);
		ImGui::TextDisabled("MODULE WORKSPACE");
		ImGui::Separator();
		if (paths.empty())
		{
			ImGui::TextDisabled("No DLL files found in ./dlls");
			ImGui::Spacing();
			ImGui::TextWrapped("Place a DLL in the dlls folder to make it available here.");
		}
		else
		{
			std::string dllItems;
			for (const auto& path : paths)
			{
				const auto separator = path.find_last_of("\\/");
				dllItems += path.substr(separator == std::string::npos ? 0 : separator + 1);
				dllItems.push_back('\0');
			}
			dllItems.push_back('\0');
			this->selectedDLL = std::clamp(this->selectedDLL, 0, static_cast<int>(paths.size()) - 1);
			ImGui::Combo("DLL", &this->selectedDLL, dllItems.c_str());
		}

		ImGui::Spacing();
		const bool canInject = !paths.empty() && !this->isInjecting;
		if (!canInject)
			ImGui::BeginDisabled();
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.52f, 0.92f, canInject ? 1.0f : 0.35f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.63f, 1.0f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.12f, 0.40f, 0.82f, 1.0f));
		if (ImGui::Button(this->isInjecting ? "Injecting..." : "Inject selected module", ImVec2(-1.0f, 42.0f)) && canInject)
		{
			bool valid = true;
			if (g_injector->isCustomProcess && mem::getProcID(g_injector->customProcessName) == NULL)
			{
				MessageBox(hwnd, L"Custom process not found...", nullptr, 0);
				valid = false;
			}
			if (valid)
				std::thread(&Injector::inject, g_injector.get(), paths[this->selectedDLL]).detach();
		}
		if (!canInject)
			ImGui::EndDisabled();
		ImGui::PopStyleColor(3);

		if (this->isInjecting)
			ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.25f, 1.0f), "Injecting module...");
	ImGui::EndChild();
}

void Menu::setupMenuStyle(bool isDarkTheme, float alpha)
{
	ImGuiStyle& style = ImGui::GetStyle();

	style = ImGuiStyle();
	style.Alpha = alpha;
	style.WindowPadding = ImVec2(10.0f, 10.0f);
	style.FramePadding = ImVec2(10.0f, 8.0f);
	style.ItemSpacing = ImVec2(10.0f, 10.0f);
	style.ItemInnerSpacing = ImVec2(7.0f, 6.0f);
	style.WindowRounding = 18.0f;
	style.ChildRounding = 12.0f;
	style.FrameRounding = 9.0f;
	style.PopupRounding = 10.0f;
	style.ScrollbarRounding = 10.0f;
	style.GrabRounding = 9.0f;
	style.TabRounding = 9.0f;
	style.WindowBorderSize = 0.0f;
	style.ChildBorderSize = 1.0f;
	style.FrameBorderSize = 0.0f;
	style.WindowTitleAlign = ImVec2(0.08f, 0.5f);

	const ImVec4 accent = isDarkTheme ? ImVec4(0.30f, 0.58f, 1.00f, 1.0f) : ImVec4(0.12f, 0.38f, 0.82f, 1.0f);
	const ImVec4 accentHover = isDarkTheme ? ImVec4(0.40f, 0.66f, 1.00f, 1.0f) : ImVec4(0.18f, 0.47f, 0.94f, 1.0f);
	const ImVec4 canvas = canvasColor(isDarkTheme);
	const ImVec4 panel = isDarkTheme ? ImVec4(0.070f, 0.090f, 0.140f, 1.0f) : ImVec4(0.985f, 0.990f, 0.998f, 1.0f);
	const ImVec4 frame = isDarkTheme ? ImVec4(0.105f, 0.135f, 0.205f, 1.0f) : ImVec4(0.900f, 0.935f, 0.975f, 1.0f);

	style.Colors[ImGuiCol_Text] = isDarkTheme ? ImVec4(0.91f, 0.94f, 0.99f, 1.0f) : ImVec4(0.10f, 0.13f, 0.19f, 1.0f);
	style.Colors[ImGuiCol_TextDisabled] = isDarkTheme ? ImVec4(0.54f, 0.60f, 0.70f, 1.0f) : ImVec4(0.42f, 0.48f, 0.57f, 1.0f);
	style.Colors[ImGuiCol_WindowBg] = canvas;
	style.Colors[ImGuiCol_ChildBg] = panel;
	style.Colors[ImGuiCol_PopupBg] = panel;
	style.Colors[ImGuiCol_Border] = isDarkTheme ? ImVec4(0.20f, 0.30f, 0.46f, 0.72f) : ImVec4(0.67f, 0.76f, 0.88f, 0.85f);
	style.Colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
	style.Colors[ImGuiCol_FrameBg] = frame;
	style.Colors[ImGuiCol_FrameBgHovered] = isDarkTheme ? ImVec4(0.15f, 0.21f, 0.32f, 1.0f) : ImVec4(0.83f, 0.89f, 0.97f, 1.0f);
	style.Colors[ImGuiCol_FrameBgActive] = isDarkTheme ? ImVec4(0.18f, 0.27f, 0.42f, 1.0f) : ImVec4(0.76f, 0.85f, 0.96f, 1.0f);
	style.Colors[ImGuiCol_TitleBg] = style.Colors[ImGuiCol_WindowBg];
	style.Colors[ImGuiCol_TitleBgActive] = style.Colors[ImGuiCol_WindowBg];
	style.Colors[ImGuiCol_MenuBarBg] = panel;
	style.Colors[ImGuiCol_ScrollbarBg] = style.Colors[ImGuiCol_WindowBg];
	style.Colors[ImGuiCol_ScrollbarGrab] = isDarkTheme ? ImVec4(0.22f, 0.34f, 0.48f, 1.0f) : ImVec4(0.64f, 0.73f, 0.84f, 1.0f);
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = accent;
	style.Colors[ImGuiCol_ScrollbarGrabActive] = accentHover;
	style.Colors[ImGuiCol_CheckMark] = accentHover;
	style.Colors[ImGuiCol_SliderGrab] = accent;
	style.Colors[ImGuiCol_SliderGrabActive] = accentHover;
	style.Colors[ImGuiCol_Button] = ImVec4(accent.x, accent.y, accent.z, 0.25f);
	style.Colors[ImGuiCol_ButtonHovered] = ImVec4(accentHover.x, accentHover.y, accentHover.z, 0.85f);
	style.Colors[ImGuiCol_ButtonActive] = accent;
	style.Colors[ImGuiCol_Header] = ImVec4(accent.x, accent.y, accent.z, 0.22f);
	style.Colors[ImGuiCol_HeaderHovered] = ImVec4(accentHover.x, accentHover.y, accentHover.z, 0.45f);
	style.Colors[ImGuiCol_HeaderActive] = ImVec4(accent.x, accent.y, accent.z, 0.65f);
	style.Colors[ImGuiCol_Separator] = style.Colors[ImGuiCol_Border];
	style.Colors[ImGuiCol_SeparatorHovered] = accentHover;
	style.Colors[ImGuiCol_SeparatorActive] = accent;
	style.Colors[ImGuiCol_NavHighlight] = accent;
	style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(accent.x, accent.y, accent.z, 0.35f);
}

void Menu::detectSteam()
{
	while (this->isMenuOn)
	{
		DWORD pID = mem::getProcID(vars::str_steam_process_name.data());
		g_injector->steamRunning = !(pID == NULL);
		std::this_thread::sleep_for(1s);
	}
}

void Menu::detectGame()
{
	while (this->isMenuOn)
	{
		DWORD pID = mem::getProcID(vars::str_game_process_name.data());
		g_injector->csgoRunning = !(pID == NULL);
		std::this_thread::sleep_for(1s);
	}
}

void Menu::updateFiles()
{
	if (!std::filesystem::is_directory(vars::str_dll_dir_path) || !std::filesystem::exists(vars::str_dll_dir_path)) { // Check if src folder exists
		std::filesystem::create_directory(vars::str_dll_dir_path); // create src folder
	}
	
	while (this->isMenuOn)
	{
		this->mtx.lock();
		this->filePaths.clear();
		for (const auto& file : std::filesystem::directory_iterator(vars::str_dll_dir_path))
		{
			if (!std::filesystem::is_directory(file) && (file.path().string().substr(file.path().string().find_last_of(".") + 1) == "dll"))
				this->filePaths.push_back(file.path().string());
		}
		this->mtx.unlock();
		std::this_thread::sleep_for(1s);
	}
}
