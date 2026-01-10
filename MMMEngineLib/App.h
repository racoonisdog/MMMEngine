#pragma once
#include <Windows.h>
#include <string>

#include "Singleton.hpp"
#include "Delegates.hpp"

namespace MMMEngine::Utility
{
	class App
	{
	public:
		struct WindowInfo
		{
			LPCWSTR title;
			LONG width;
			LONG height;
			LONG style;
		};

		App();
		~App();

		int Run();
		void Quit();

		Event<App, void(void)> OnIntialize{ this };
		Event<App, void(void)> OnShutdown{ this };
		Event<App, void(void)> OnUpdate{ this };
		Event<App, void(void)> OnRender{ this };
		Event<App, void(int,int)> OnResize{ this };

		void SetProcessHandle(HINSTANCE hinstance);
		WindowInfo GetWindowInfo();
		HWND GetWindowHandle();
	protected:
		LRESULT HandleWindowMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	private:
		HINSTANCE m_hInstance;
		HWND m_hWnd;
		bool m_isRunning;

		bool m_needResizeWindow;

		WindowInfo m_windowInfo;

		bool CreateMainWindow();
		static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	};
}