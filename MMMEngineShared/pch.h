#pragma once

#ifndef NOMINMAX
#define NOMINMAX // STL의 std::min, max와 충돌하는 windows.h의 min, max 매크로 제거
#endif

#define WIN32_LEAN_AND_MEAN // 거의 사용되지 않는 네트워크, DDE, RPC, GDI 등을 제외하여 헤더 크기 축소
#define VC_EXTRALEAN        // MFC 관련 불필요한 리소스를 제외하여 컴파일 속도 향상

#include <Windows.h>

#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include "rttr/type"
#include "rttr/registration"
