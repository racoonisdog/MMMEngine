#pragma once

namespace MMMEngine
{
    enum class KeyCode
    {
        // 알파벳
        A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

        // 숫자
        Alpha0, Alpha1, Alpha2, Alpha3, Alpha4, Alpha5, Alpha6, Alpha7, Alpha8, Alpha9,

        // 특수 키
        Escape, Space, Enter, Tab, Backspace, Delete,
        LeftShift, RightShift, LeftControl, RightControl, LeftAlt, RightAlt,
        UpArrow, DownArrow, LeftArrow, RightArrow,

        Comma, Period, Slash, Semicolon, Quote, LeftBracket, RightBracket, Minus, Equals,

        F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

        // 마우스 버튼
        MouseLeft, MouseRight, MouseMiddle,

        // 기타
        Unknown,
    };
}