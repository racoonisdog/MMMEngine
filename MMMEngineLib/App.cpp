#include "App.h"
#include <wrl/client.h>


MMMEngine::Utility::App::App()
	: m_hInstance(GetModuleHandle(NULL))
	, m_hWnd(NULL)
	, m_isRunning(false)
	, m_needResizeWindow(false)
	, m_windowInfo({ L"MMM Engine Application" , 1600, 900})
{
}

MMMEngine::Utility::App::~App()
{

}

int MMMEngine::Utility::App::Run()
{
	if (FAILED(CoInitialize(nullptr)))
		return -1;

	if (!CreateMainWindow())
		return -2;

	m_isRunning = true;
	OnIntialize(this);
	MSG msg = {};
	while (m_isRunning && msg.message != WM_QUIT)
	{
		if (m_needResizeWindow)
		{
			OnResize(this, m_windowInfo.width, m_windowInfo.height);
			m_needResizeWindow = false;
		}

		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			OnUpdate(this);
			OnRender(this);
		}
	}
	OnShutdown(this);

	CoUninitialize();
	return (int)msg.wParam;
}

void MMMEngine::Utility::App::Quit()
{
	m_isRunning = false;
	if (m_hWnd)
		PostMessageW(m_hWnd, WM_CLOSE, 0, 0);
	else
		PostQuitMessage(0);
}

void MMMEngine::Utility::App::SetProcessHandle(HINSTANCE hinstance)
{
	m_hInstance = hinstance;
}

MMMEngine::Utility::App::WindowInfo MMMEngine::Utility::App::GetWindowInfo()
{
	return m_windowInfo;
}

HWND MMMEngine::Utility::App::GetWindowHandle()
{
	return m_hWnd;
}

LRESULT MMMEngine::Utility::App::HandleWindowMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	//#ifdef _DEBUG
	//if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
	//	return true;
	//#endif //_DEBUG

	switch (uMsg) {
	case WM_CLOSE:
		DestroyWindow(hWnd);
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_SIZE:
		if (wParam == SIZE_MINIMIZED) return 0;
		if (/*m_pD3DContext && */(wParam == SIZE_MAXIMIZED || wParam == SIZE_RESTORED)) {
			RECT clientRect;
			GetClientRect(hWnd, &clientRect);
			LONG newWidth = clientRect.right - clientRect.left;
			LONG newHeight = clientRect.bottom - clientRect.top;

			// 리사이즈 로직을 D3DContext에 위임
			if (newWidth != m_windowInfo.width || newHeight != m_windowInfo.height) {
				m_windowInfo.width = newWidth;
				m_windowInfo.height = newHeight;
				m_needResizeWindow = true;
			}
		}
		break;
	default:
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
	return 0;
}

bool MMMEngine::Utility::App::CreateMainWindow()
{
	// 윈도우 클래스 정의
	WNDCLASSEX wc = { sizeof(WNDCLASSEX) };
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = m_hInstance;
	wc.lpszClassName = L"MMMEngineClass";
	ATOM a = RegisterClassExW(&wc);
	if (a == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

	// 클라이언트 영역에 맞춰 윈도우 전체 크기 계산
	RECT clientRect = { 0, 0, m_windowInfo.width, m_windowInfo.height };
	AdjustWindowRect(&clientRect, WS_OVERLAPPEDWINDOW, FALSE);

	// 윈도우 생성
	m_hWnd = CreateWindowEx(
		0,
		wc.lpszClassName,
		m_windowInfo.title,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		clientRect.right - clientRect.left,
		clientRect.bottom - clientRect.top,
		NULL,
		NULL,
		m_hInstance,
		this
	);
	if (!m_hWnd) return false;

	ShowWindow(m_hWnd, SW_SHOW);
	return true;
}

LRESULT MMMEngine::Utility::App::WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_NCCREATE) {
		auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
		SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}

	App* pApp = reinterpret_cast<App*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
	if (pApp) {
		return pApp->HandleWindowMessage(hWnd, uMsg, wParam, lParam);
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}