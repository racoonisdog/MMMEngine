#include "UserScriptMessageSignatures.h"

namespace MMMEngine
{
	std::vector<UserScriptMessageSignature> GetEngineMessageSignatures()
	{
		return {
			// void()
			{ "Awake", {} },
			{ "OnEnable", {} },
			{ "Start", {} },
			{ "OnDisable", {} },
			{ "OnDestroy", {} },
			{ "FixedUpdate", {} },
			{ "Update", {} },
			{ "LateUpdate", {} },
			{ "OnSceneLoaded", {} },
			// void(CollisionInfo) / void(const CollisionInfo&) 등 → param "CollisionInfo"
			{ "OnCollisionEnter", { "CollisionInfo" } },
			{ "OnCollisionStay", { "CollisionInfo" } },
			{ "OnCollisionExit", { "CollisionInfo" } },
			// void(TriggerInfo) / void(const TriggerInfo&) 등 → param "TriggerInfo"
			{ "OnTriggerEnter", { "TriggerInfo" } },
			{ "OnTriggerExit", { "TriggerInfo" } },
		};
	}
}
