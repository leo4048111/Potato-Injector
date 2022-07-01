#pragma once
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

	void detectSteam();

	void detectGame();

	void updateFiles();

private:
	LPDIRECT3D9              pD3D = NULL;
	LPDIRECT3DDEVICE9        d3dDevice = NULL;
	D3DPRESENT_PARAMETERS    d3dpp = {};
	HWND					 hwnd{ NULL };

	bool isMenuOn{ false };
	bool isPatchingVac{ false };
	bool isInjecting{ false };

	std::vector<std::string> filePaths;

	std::mutex mtx;

};

inline auto g_menu = std::make_unique<Menu>();

