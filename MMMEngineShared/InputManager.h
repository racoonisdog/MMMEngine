#pragma once
#include "Export.h"
#include "ExportSingleton.hpp"
#include "InputConstant.h"
#include "InputKeyCode.h"
#include <Windows.h>
#include <unordered_map>

#include "SimpleMath.h"

namespace MMMEngine
{
    class MMMENGINE_API InputManager : public Utility::ExportSingleton<InputManager>
    {
    private:
        HWND m_hWnd; // 윈도우 핸들
        POINT m_mouseClient; // 마우스 좌표
        POINT m_prevMouseClient;  // 이전 프레임 마우스 좌표
#pragma warning(push)
#pragma warning(disable: 4251)
        DirectX::SimpleMath::Vector2 m_mouseDelta;
#pragma warning(pop)
        SHORT m_prevState[256] = { 0 };
        SHORT m_currState[256] = { 0 };

        RECT m_clientRect;

        //// 게임패드 관리 (최대 4개)
        //std::array<XInputGamepadDevice*, 4> m_gamepads;
        //void InitGamepads(); // 게임패드 초기화
        //void CleanupGamepads(); // 게임패드 정리
    public:
        InputManager() = default;
        ~InputManager() = default;

        void HandleWindowResize(int newWidth, int newHeight);

        DirectX::SimpleMath::Vector2 GetMousePos();
        bool GetKey(KeyCode keyCode);
        bool GetKeyDown(KeyCode keyCode);
        bool GetKeyUp(KeyCode keyCode);
        DirectX::SimpleMath::Vector2 GetMouseDelta();

        void StartUp(HANDLE windowHandle);
        void ShutDown();
        void Update();
    };
}