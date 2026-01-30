#pragma once

#include "Export.h"
#include <string>
#include <vector>

namespace MMMEngine
{
	/// 빌드툴에서 유저 스크립트 메시지 등록 여부를 판단할 때 쓰는 엔진 기본 시그니처.
	/// - 시그니처가 이 목록과 일치하면 매크로 없이도 자동 등록.
	/// - 매크로(USCRIPT_MESSAGE / USCRIPT_MESSAGE_NAME)를 쓴 경우엔 목록 여부와 관계없이 등록.
	struct UserScriptMessageSignature
	{
		std::string messageName;
		/// 정규화된 파라미터 타입 문자열 목록 (예: "CollisionInfo", "TriggerInfo"). void()면 빈 벡터.
		std::vector<std::string> paramTypes;
	};

	/// 엔진에서 브로드캐스트하는 메시지 시그니처 목록 (빌드툴 하드코딩용).
	MMMENGINE_API std::vector<UserScriptMessageSignature> GetEngineMessageSignatures();
}
