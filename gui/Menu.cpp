#include "pch.h"
#include "Menu.hpp"

#include <algorithm>

#include "dependency/imgui/imgui.h"
#include "dependency/imgui/imgui_internal.h"
#include "dependency/imgui/backend/imgui_impl_dx9.h"
#include "dependency/imgui/backend/imgui_impl_win32.h"

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

bool Menu::initialize()
{
	// Create application window
	//ImGui_ImplWin32_EnableDpiAwareness();
	WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, Menu::WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("WC"), NULL };
	::RegisterClassEx(&wc);
	this->hwnd = ::CreateWindow(wc.lpszClassName, _T("Potato Injector"),
		WS_POPUP | WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX,
		100, 100, 360, 430, NULL, NULL, wc.hInstance, NULL);
	::SetWindowLong(hwnd, GWL_STYLE, GetWindowLong(hwnd, GWL_STYLE)
		& WS_CAPTION & ~WS_THICKFRAME);
	::SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);

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
	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

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

		ImGui::SetNextWindowSize({ 360, 430 });
		ImGui::SetNextWindowPos({ 0, 0 });
		ImGui::Begin("Potato Injector", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
		ImGui::TextColored(ImVec4(0.35f, 0.70f, 1.00f, 1.00f), "Potato Injector");
		ImGui::SameLine();
		ImGui::TextDisabled(" / control panel");
		ImGui::Separator();

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
		D3DCOLOR clear_col_dx = D3DCOLOR_RGBA((int)(clear_color.x * clear_color.w * 255.0f), (int)(clear_color.y * clear_color.w * 255.0f), (int)(clear_color.z * clear_color.w * 255.0f), (int)(clear_color.w * 255.0f));
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
	if (ImGui::CollapsingHeader("STATUS", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::BeginChild("StatusPanel", ImVec2(0, 78), true);
		const auto status = [](const char* label, bool active, const char* activeText, const char* inactiveText)
		{
			ImGui::TextUnformatted(label);
			ImGui::SameLine(122.0f);
			ImGui::PushStyleColor(ImGuiCol_Text, active ? ImVec4(0.35f, 0.85f, 0.45f, 1.0f) : ImVec4(0.95f, 0.40f, 0.40f, 1.0f));
			ImGui::Text("%s", active ? activeText : inactiveText);
			ImGui::PopStyleColor();
		};

		status("Steam", g_injector->steamRunning, "RUNNING", "OFFLINE");
		status("CS2", g_injector->csgoRunning, this->isInjecting ? "INJECTING" : "RUNNING", "OFFLINE");
		ImGui::TextDisabled("VAC3 patching is disabled");
		ImGui::EndChild();
	}
}

void Menu::renderTargetPanel()
{
	if (ImGui::CollapsingHeader("TARGET", ImGuiTreeNodeFlags_DefaultOpen))
	{
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
	}
}

std::vector<std::string> Menu::snapshotDllPaths()
{
	std::scoped_lock lock(this->mtx);
	return this->filePaths;
}

void Menu::renderInjectionPanel(const std::vector<std::string>& paths)
{
	if (ImGui::CollapsingHeader("MODULE", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (paths.empty())
		{
			ImGui::TextDisabled("No DLL files found in ./dlls");
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
		if (ImGui::Button("Inject", ImVec2(-1.0f, 34.0f)) && canInject)
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

		if (this->isInjecting)
			ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.25f, 1.0f), "Injecting module...");
	}
}

void Menu::setupMenuStyle(bool isDarkTheme, float alpha)
{
	ImGuiStyle& style = ImGui::GetStyle();

	style.Alpha = 1.0f;
	style.WindowPadding = ImVec2(16.0f, 14.0f);
	style.FramePadding = ImVec2(9.0f, 7.0f);
	style.ItemSpacing = ImVec2(8.0f, 8.0f);
	style.ItemInnerSpacing = ImVec2(6.0f, 5.0f);
	style.WindowRounding = 10.0f;
	style.ChildRounding = 8.0f;
	style.FrameRounding = 6.0f;
	style.PopupRounding = 6.0f;
	style.ScrollbarRounding = 8.0f;
	style.GrabRounding = 6.0f;
	style.WindowBorderSize = 1.0f;
	style.ChildBorderSize = 1.0f;
	style.FrameBorderSize = 0.0f;
	style.Colors[ImGuiCol_Text] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
	style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
	style.Colors[ImGuiCol_WindowBg] = ImVec4(0.94f, 0.94f, 0.94f, 0.94f);
	style.Colors[ImGuiCol_PopupBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.94f);
	style.Colors[ImGuiCol_Border] = ImVec4(0.00f, 0.00f, 0.00f, 0.39f);
	style.Colors[ImGuiCol_BorderShadow] = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);
	style.Colors[ImGuiCol_FrameBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.94f);
	style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
	style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
	style.Colors[ImGuiCol_TitleBg] = ImVec4(0.96f, 0.96f, 0.96f, 1.00f);
	style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(1.00f, 1.00f, 1.00f, 0.51f);
	style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);
	style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
	style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.98f, 0.98f, 0.98f, 0.53f);
	style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.69f, 0.69f, 0.69f, 1.00f);
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.59f, 0.59f, 0.59f, 1.00f);
	style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.49f, 0.49f, 0.49f, 1.00f);
	style.Colors[ImGuiCol_CheckMark] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);
	style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	style.Colors[ImGuiCol_Button] = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
	style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
	style.Colors[ImGuiCol_Header] = ImVec4(0.26f, 0.59f, 0.98f, 0.31f);
	style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
	style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	style.Colors[ImGuiCol_ResizeGrip] = ImVec4(1.00f, 1.00f, 1.00f, 0.50f);
	style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
	style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
	style.Colors[ImGuiCol_PlotLines] = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
	style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
	style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
	style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
	style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);

	if (isDarkTheme)
	{
		for (int i = 0; i <= ImGuiCol_COUNT; i++)
		{
			ImVec4& col = style.Colors[i];
			float H, S, V;
			ImGui::ColorConvertRGBtoHSV(col.x, col.y, col.z, H, S, V);

			if (S < 0.1f)
			{
				V = 1.0f - V;
			}
			ImGui::ColorConvertHSVtoRGB(H, S, V, col.x, col.y, col.z);
			if (col.w < 1.00f)
			{
				col.w *= alpha;
			}
		}
	}
	else
	{
		for (int i = 0; i <= ImGuiCol_COUNT; i++)
		{
			ImVec4& col = style.Colors[i];
			if (col.w < 1.00f)
			{
				col.x *= alpha;
				col.y *= alpha;
				col.z *= alpha;
				col.w *= alpha;
				col.w *= alpha;
			}
		}
	}
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
