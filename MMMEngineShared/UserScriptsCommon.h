#pragma once

#ifdef USERSCRIPTS_EXPORT
#define USERSCRIPTS __declspec(dllexport)
#else
#define USERSCRIPTS __declspec(dllimport)
#endif

// =============================================================================
// 유저 스크립트 메시지 등록 규칙 (빌드툴 / 헤더 분석기 기준)
// =============================================================================
// 1. 매크로 없음: 엔진 하드코딩 시그니처(GetEngineMessageSignatures())와 이름·파라미터가
//    일치하면 시그니처만으로 자동 등록 (REGISTER_BEHAVIOUR_MESSAGE 등).
// 2. 매크로 사용: USCRIPT_MESSAGE() / USCRIPT_MESSAGE_NAME("이름") 을 붙이면
//    하드코딩 여부와 관계없이 항상 등록 (커스텀 메시지 포함).
// =============================================================================

/// 메시지 이름 = 함수 이름으로 등록. (빌드툴에서 인식용, C++에서는 빈 매크로)
#define USCRIPT_MESSAGE()

/// 메시지 이름을 커스텀하여 등록. (빌드툴에서 인식용, C++에서는 빈 매크로)
#define USCRIPT_MESSAGE_NAME(name)

/// 생성자 안에서만 호출 가능. 실행 순서 설정 (생성기가 해당 라인 유지)
#define USCRIPT_EXECUTION_ORDER(order) SetExecutionOrder(order)

/// 인스펙터/리플렉션에 노출할 프로퍼티 표시 (빌드툴에서 gen.cpp 프로퍼티 등록용)
#define USCRIPT_PROPERTY()

/// 프로퍼티이지만 자동 등록 대상에서 제외 (빌드툴 스킵용)
#define USCRIPT_PROPERTY_HIDDEN()

/// 이 클래스는 빌드툴 자동 생성 스킵 (리플렉션/생성자 등 직접 작성 시)
#define USCRIPT_DONT_AUTOGEN()