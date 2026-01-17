#include "App.h"
#include <wrl/client.h>
#include <algorithm>

MMMEngine::Utility::App::App()
	: m_hInstance(GetModuleHandle(NULL))
	, m_hWnd(NULL)
	, m_isRunning(false)
	, m_windowSizeDirty(false)
	, m_currentDisplayMode(DisplayMode::Windowed)
	, m_previousDisplayMode(DisplayMode::Windowed)
	, m_windowInfo({ L"MMM Engine Application",1600,900,WS_OVERLAPPEDWINDOW})
{
}

MMMEngine::Utility::App::App(HINSTANCE hInstance)
	: m_hInstance(hInstance)
	, m_hWnd(NULL)
	, m_isRunning(false)
	, m_windowSizeDirty(false)
	, m_currentDisplayMode(DisplayMode::Windowed)
	, m_previousDisplayMode(DisplayMode::Windowed)
	, m_windowInfo({ L"MMM Engine Application",1600,900,WS_OVERLAPPEDWINDOW})
{
}

MMMEngine::Utility::App::App(LPCWSTR title, LONG width, LONG height)
	: m_hInstance(GetModuleHandle(NULL))
	, m_hWnd(NULL)
	, m_isRunning(false)
	, m_windowSizeDirty(false)
	, m_currentDisplayMode(DisplayMode::Windowed)
	, m_previousDisplayMode(DisplayMode::Windowed)
	, m_windowInfo({ title,width,height,WS_OVERLAPPEDWINDOW })
{
}

MMMEngine::Utility::App::App(HINSTANCE hInstance, LPCWSTR title, LONG width, LONG height)
	: m_hInstance(hInstance)
	, m_hWnd(NULL)
	, m_isRunning(false)
	, m_windowSizeDirty(false)
	, m_currentDisplayMode(DisplayMode::Windowed)
	, m_previousDisplayMode(DisplayMode::Windowed)
	, m_windowInfo({ title,width,height,WS_OVERLAPPEDWINDOW })
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
	OnInitialize(this);
	MSG msg = {};
	while (m_isRunning && msg.message != WM_QUIT)
	{
		if (m_windowSizeDirty)
		{
			OnWindowSizeChanged(this, m_windowInfo.width, m_windowInfo.height);
			m_windowSizeDirty = false;
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
	OnRelease(this);

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

const MMMEngine::Utility::App::WindowInfo MMMEngine::Utility::App::GetWindowInfo() const
{
	return m_windowInfo;
}

HWND MMMEngine::Utility::App::GetWindowHandle() const
{
	return m_hWnd;
}

void MMMEngine::Utility::App::SetWindowSize(int width, int height)
{
	if (!m_hWnd)
	{
		return;
	}

	RECT windowRect = { 0, 0, width, height };

	BOOL success = ::AdjustWindowRect(&windowRect, m_windowInfo.style, FALSE);

	if (success)
	{
		LONG adjustedWidth = windowRect.right - windowRect.left;
		LONG adjustedHeight = windowRect.bottom - windowRect.top;

		::SetWindowPos(
			m_hWnd,
			NULL,               // Z-Order 변경 안 함
			0, 0,               // SWP_NOMOVE 플래그를 사용하므로 X, Y는 무시됨
			adjustedWidth,      // 계산된 전체 폭
			adjustedHeight,     // 계산된 전체 높이
			SWP_NOZORDER | SWP_NOMOVE | SWP_FRAMECHANGED
		);
	}
	
}

void MMMEngine::Utility::App::SetResizable(bool isResizable)
{
	if (m_isResizable == isResizable)
		return; // 변경 사항이 없으면 종료

	m_isResizable = isResizable;

	if (m_hWnd && m_currentDisplayMode == DisplayMode::Windowed)
	{
		// 현재 윈도우 스타일을 가져옵니다.
		DWORD currentStyle = GetWindowLong(m_hWnd, GWL_STYLE);

		if (isResizable)
		{
			// 크기 조절을 허용하려면 WS_THICKFRAME 플래그를 추가합니다.
			currentStyle |= WS_THICKFRAME;
		}
		else
		{
			// 크기 조절을 막으려면 WS_THICKFRAME 플래그를 제거합니다.
			currentStyle &= ~WS_THICKFRAME;
		}

		// 새로운 스타일을 적용합니다.
		SetWindowLong(m_hWnd, GWL_STYLE, currentStyle);

		// 변경 사항을 적용하고 창의 비클라이언트 영역을 다시 그리도록 합니다.
		SetWindowPos(
			m_hWnd,
			NULL,
			0, 0,
			0, 0,
			SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE
		);
	}
}

std::vector<MMMEngine::Resolution> MMMEngine::Utility::App::GetCurrentMonitorResolutions() const
{
	return GetMonitorResolutionsFromWindow(m_hWnd);
}

std::vector<MMMEngine::Resolution> MMMEngine::Utility::App::GetMonitorResolutionsFromWindow(HWND hWnd)
{
	std::vector<Resolution> resolutions;

	if (!hWnd)
		return resolutions;

	// 1) 현재 윈도우가 위치한 모니터
	HMONITOR hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
	if (!hMonitor)
		return resolutions;

	// 2) 모니터 디바이스 이름 얻기
	MONITORINFOEXW mi{};
	mi.cbSize = sizeof(mi);
	if (!GetMonitorInfoW(hMonitor, &mi))
		return resolutions;

	// 3) 디스플레이 모드 열거
	DEVMODEW dm{};
	dm.dmSize = sizeof(dm);

	for (DWORD i = 0; EnumDisplaySettingsW(mi.szDevice, i, &dm); ++i)
	{
		Resolution r{
			static_cast<int>(dm.dmPelsWidth),
			static_cast<int>(dm.dmPelsHeight)
		};

		// 중복 제거
		if (std::find(resolutions.begin(), resolutions.end(), r) == resolutions.end())
		{
			resolutions.push_back(r);
		}
	}

	// 4) 정렬 (가로 → 세로)
	std::sort(resolutions.begin(), resolutions.end(),
		[](const Resolution& a, const Resolution& b)
		{
			if (a.width != b.width)
				return a.width < b.width;
			return a.height < b.height;
		});

	return resolutions;
}

LRESULT MMMEngine::Utility::App::HandleWindowMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	OnBeforeWindowMessage(this, hWnd, uMsg, wParam, lParam);

	LRESULT result = 0;

	switch (uMsg) {
	//case WM_SETCURSOR:
	//	// 클라이언트 영역에서 기본 화살표 커서 설정
	//	if (LOWORD(lParam) == HTCLIENT)
	//	{
	//		SetCursor(LoadCursor(NULL, IDC_ARROW));
	//		return TRUE;
	//	}
	//	// 비클라이언트 영역은 기본 처리
	//	result = DefWindowProc(hWnd, uMsg, wParam, lParam);
	//	break;
	case WM_SYSKEYDOWN:  // Alt + 다른 키 조합
		if (wParam == VK_RETURN && (lParam & 0x60000000) == 0x20000000)  // Alt+Enter
		{
			ToggleFullscreen();
			break; // 메시지 처리 완료
		}
		break;
	case WM_CLOSE:
		DestroyWindow(hWnd);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_SIZE:
		if (wParam != SIZE_MINIMIZED &&
			(wParam == SIZE_MAXIMIZED || wParam == SIZE_RESTORED)) {

			RECT clientRect;
			GetClientRect(hWnd, &clientRect);
			LONG newWidth = clientRect.right - clientRect.left;
			LONG newHeight = clientRect.bottom - clientRect.top;

			// 리사이즈 로직을 D3DContext에 위임
			if (newWidth != m_windowInfo.width || newHeight != m_windowInfo.height) {
				m_windowInfo.width = newWidth;
				m_windowInfo.height = newHeight;
				m_windowSizeDirty = true;
			}
		}
		break;
	default:
		result = DefWindowProc(hWnd, uMsg, wParam, lParam);
		break;
	}

	OnAfterWindowMessage(this, hWnd, uMsg, wParam, lParam);

	return result;
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
		m_windowInfo.title.c_str(),
		m_windowInfo.style,
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

void MMMEngine::Utility::App::SetWindowTitle(const std::wstring& title)
{
	m_windowInfo.title = title;

	// 윈도우가 이미 생성되어 있으면 즉시 제목 변경
	if (m_hWnd)
	{
		SetWindowTextW(m_hWnd, m_windowInfo.title.c_str());
	}
}

void MMMEngine::Utility::App::SetDisplayMode(DisplayMode mode)
{
	if (m_currentDisplayMode == mode)
		return;

	m_previousDisplayMode = m_currentDisplayMode;
	m_currentDisplayMode = mode;

	switch (mode)
	{
	case DisplayMode::Windowed:
		SetWindowed();
		break;
	case DisplayMode::BorderlessWindowed:
		SetBorderlessWindowed();
		break;
	case DisplayMode::Fullscreen:
		SetFullscreen();
		break;
	}
}

MMMEngine::DisplayMode MMMEngine::Utility::App::GetDisplayMode() const
{
	return m_currentDisplayMode;
}

void MMMEngine::Utility::App::ToggleFullscreen()
{
	if (m_currentDisplayMode == DisplayMode::Fullscreen)
	{
		SetDisplayMode(DisplayMode::Windowed);
	}
	else
	{
		SetDisplayMode(DisplayMode::Fullscreen);
	}
}

void MMMEngine::Utility::App::SaveWindowedState()
{
	m_windowedRestore.style = GetWindowLong(m_hWnd, GWL_STYLE);
	m_windowedRestore.exStyle = GetWindowLong(m_hWnd, GWL_EXSTYLE);
	GetWindowRect(m_hWnd, &m_windowedRestore.rect);
	m_windowedRestore.valid = true;
}

void MMMEngine::Utility::App::RestoreWindowedState()
{
	if (!m_windowedRestore.valid)
		return;

	SetWindowLong(m_hWnd, GWL_STYLE, m_windowedRestore.style);
	SetWindowLong(m_hWnd, GWL_EXSTYLE, m_windowedRestore.exStyle);

	SetWindowPos(m_hWnd, HWND_NOTOPMOST,
		m_windowedRestore.rect.left,
		m_windowedRestore.rect.top,
		m_windowedRestore.rect.right - m_windowedRestore.rect.left,
		m_windowedRestore.rect.bottom - m_windowedRestore.rect.top,
		SWP_FRAMECHANGED | SWP_SHOWWINDOW);
}

void MMMEngine::Utility::App::SetWindowed()
{
	if (m_previousDisplayMode == DisplayMode::Fullscreen)
	{
		ChangeDisplaySettings(NULL, 0);
	}

	if (m_currentDisplayMode != DisplayMode::Windowed)
	{
		// 이전 상태 복원
		RestoreWindowedState();
	}
	else
	{
		// 일반 창 스타일 설정
		DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
		SetWindowLong(m_hWnd, GWL_STYLE, style);

		RECT rect = { 0, 0, m_windowInfo.width, m_windowInfo.height };
		AdjustWindowRect(&rect, style, FALSE);

		int centerX = (GetSystemMetrics(SM_CXSCREEN) - (rect.right - rect.left)) / 2;
		int centerY = (GetSystemMetrics(SM_CYSCREEN) - (rect.bottom - rect.top)) / 2;

		SetWindowPos(m_hWnd, HWND_NOTOPMOST,
			centerX, centerY,
			rect.right - rect.left,
			rect.bottom - rect.top,
			SWP_FRAMECHANGED | SWP_SHOWWINDOW);
	}
}

void MMMEngine::Utility::App::SetBorderlessWindowed()
{
	if (m_currentDisplayMode == DisplayMode::Windowed)
		SaveWindowedState();

	DWORD style = WS_POPUP | WS_VISIBLE;
	SetWindowLong(m_hWnd, GWL_STYLE, style);
	SetWindowLong(m_hWnd, GWL_EXSTYLE, WS_EX_APPWINDOW);

	HMONITOR hMonitor = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);
	MONITORINFO monitorInfo = { sizeof(MONITORINFO) };
	GetMonitorInfo(hMonitor, &monitorInfo);

	const RECT& fullArea = monitorInfo.rcMonitor;

	int x = fullArea.left;
	int y = fullArea.top;
	int width = fullArea.right - fullArea.left;
	int height = fullArea.bottom - fullArea.top;

	SetWindowPos(m_hWnd, HWND_TOP, x, y, width, height,
		SWP_FRAMECHANGED | SWP_SHOWWINDOW);

	if (m_windowInfo.width != width || m_windowInfo.height != height) {
		m_windowInfo.width = width;
		m_windowInfo.height = height;
	}
}

void MMMEngine::Utility::App::SetFullscreen()
{
	if (m_currentDisplayMode == DisplayMode::Windowed)
		SaveWindowedState();

	DWORD style = WS_POPUP | WS_VISIBLE;
	SetWindowLong(m_hWnd, GWL_STYLE, style);
	SetWindowLong(m_hWnd, GWL_EXSTYLE, WS_EX_APPWINDOW | WS_EX_TOPMOST);

	DEVMODE dm = {};
	dm.dmSize = sizeof(dm);
	dm.dmPelsWidth = m_windowInfo.width;
	dm.dmPelsHeight = m_windowInfo.height;
	dm.dmBitsPerPel = 32;
	dm.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
	ChangeDisplaySettings(&dm, CDS_FULLSCREEN);

	SetWindowPos(m_hWnd, HWND_TOPMOST, 0, 0, m_windowInfo.width, m_windowInfo.height,
		SWP_FRAMECHANGED | SWP_SHOWWINDOW);
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