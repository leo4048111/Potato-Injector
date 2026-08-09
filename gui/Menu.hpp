#pragma once
#include <atomic>

class Menu
{
	friend Injector;
public:
	Menu() = default;
	~Menu() = default;

	bool initialize();

	void loop();

private:
	bool createD3D9Device(HWND hWnd);

	void cleanupD3D9Device();

	static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	void setupMenuStyle(bool isDarkTheme, float alpha);

	void renderStatusPanel();

	void renderTargetPanel();

	std::vector<std::string> snapshotDllPaths();

	void renderInjectionPanel(const std::vector<std::string>& paths);

	void detectSteam();

	void detectGame();

	void updateFiles();

private:
	LPDIRECT3D9              pD3D = NULL;
	LPDIRECT3DDEVICE9        d3dDevice = NULL;
	D3DPRESENT_PARAMETERS    d3dpp = {};
	HWND					 hwnd{ NULL };

	std::atomic_bool isMenuOn{ false };
	std::atomic_bool isInjecting{ false };
	bool isDarkTheme{ true };

	std::vector<std::string> filePaths;
	int selectedProcess{ 0 };
	int selectedDLL{ 0 };

	std::mutex mtx;

};

inline auto g_menu = std::make_unique<Menu>();

