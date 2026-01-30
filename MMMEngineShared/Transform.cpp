#include "Transform.h"
#include "StringHelper.h"
#include "rttr/registration"
#include "GameObject.h"

using namespace DirectX::SimpleMath;
using namespace MMMEngine::Utility;

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	using Vec3Fn = void (Transform::*)(const Vector3&);		//오버로드 때문에 구현 = (float,float,float) , (Vector3) 중 누굴 호출할지 모호함 -> RTTR에러!
	using QuatFn = void (Transform::*)(const Quaternion&);  //마찬가지로 오버로드 때문에 구현
	Vec3Fn setpos = &Transform::SetLocalPosition;		
	QuatFn setrot = &Transform::SetLocalRotation;
	Vec3Fn setscale = &Transform::SetLocalScale;

	registration::class_<Transform>("Transform")
		(rttr::metadata("wrapper_type_name", "ObjPtr<Transform>"))
		(rttr::metadata("INSPECTOR", "DONT_ADD_COMP"))
		.property("Position", &Transform::GetLocalPosition, setpos)
		.property("Rotation", &Transform::GetLocalRotation, setrot)
		.property("Scale", &Transform::GetLocalScale, setscale)
		.property("Parent", &Transform::m_parent, registration::private_access)(rttr::policy::prop::as_reference_wrapper);// (rttr::metadata("INSPECTOR", "HIDDEN"));

	registration::class_<ObjPtr<Transform>>("ObjPtr<Transform>")
		.constructor<>(
			[]() {
				return Object::NewObject<Transform>();
			})
		.method("Inject", &ObjPtr<Transform>::Inject);
}

void MMMEngine::Transform::AddChild(ObjPtr<Transform> child)
{
	for (const auto& c : m_childs)
	{
		if (c == child)
			return;
	}

	m_childs.push_back(child);
}

void MMMEngine::Transform::RemoveChild(ObjPtr<Transform> child)
{
	auto it = std::find(m_childs.begin(), m_childs.end(), child);
	if (it != m_childs.end())
		m_childs.erase(it);
}

void MMMEngine::Transform::MarkDirty()
{
	m_isLocalMatDirty = true;
	m_isWorldMatDirty = true;
	for (const auto& child : m_childs)
	{
		child->MarkDirty(); // 자식들도 더러워졌다고 표시
	}
}

MMMEngine::Transform::Transform()
	: m_localPosition(0.0f, 0.0f, 0.0f)
	, m_localRotation(0.0f, 0.0f, 0.0f, 1.0f)
	, m_localScale(1.0f, 1.0f, 1.0f)
	, m_parent()
	, m_isLocalMatDirty(true)
	, m_isWorldMatDirty(true)
	, m_cachedLocalMat(Matrix::Identity)
	, m_cachedWorldMat(Matrix::Identity)
{

}

//void MMMEngine::Transform::UnInitialize()
//{
//	DetachChildren();
//	SetParent(nullptr);
//}

const Matrix& MMMEngine::Transform::GetLocalMatrix() const
{
	if (m_isLocalMatDirty)
	{
		m_cachedLocalMat =
			Matrix::CreateScale(m_localScale) *
			Matrix::CreateFromQuaternion(m_localRotation) *
			Matrix::CreateTranslation(m_localPosition);

		m_isLocalMatDirty = false;
	}

	return m_cachedLocalMat;
}

const Matrix& MMMEngine::Transform::GetWorldMatrix() const
{
	if (m_isWorldMatDirty)
	{
		if (m_parent)
			m_cachedWorldMat = GetLocalMatrix() * m_parent->GetWorldMatrix();
		else
			m_cachedWorldMat = GetLocalMatrix();

		m_isWorldMatDirty = false;
	}
	return m_cachedWorldMat;
}

const Vector3& MMMEngine::Transform::GetLocalPosition() const
{
	return m_localPosition;
}

const Quaternion& MMMEngine::Transform::GetLocalRotation() const
{
	return m_localRotation;
}

const Vector3 MMMEngine::Transform::GetLocalEulerRotation() const
{
	auto euler = m_localRotation.ToEuler();
	euler.x = DirectX::XMConvertToDegrees(euler.x);
	euler.y = DirectX::XMConvertToDegrees(euler.y);
	euler.z = DirectX::XMConvertToDegrees(euler.z);
	return euler;
}

const Vector3& MMMEngine::Transform::GetLocalScale() const
{
	return m_localScale;
}

const Vector3 MMMEngine::Transform::GetWorldPosition() const
{
	if (!m_parent) return m_localPosition;

	const Vector3 pPos = m_parent->GetWorldPosition();
	const Quaternion pRot = m_parent->GetWorldRotation();
	const Vector3 pScale = m_parent->GetWorldScale();

	Vector3 scaled = Vector3(m_localPosition.x * pScale.x,
		m_localPosition.y * pScale.y,
		m_localPosition.z * pScale.z);

	Vector3 rotated = Vector3::Transform(scaled, pRot);
	return pPos + rotated;
}

const Quaternion MMMEngine::Transform::GetWorldRotation() const
{
	if (m_parent)
	{
		return m_parent->GetWorldRotation() * m_localRotation;
	}
	return m_localRotation;
}

const Vector3 MMMEngine::Transform::GetWorldEulerRotation() const
{
	auto worldRot = GetWorldRotation();
	auto euler = worldRot.ToEuler();
	euler.x = DirectX::XMConvertToDegrees(euler.x);
	euler.y = DirectX::XMConvertToDegrees(euler.y);
	euler.z = DirectX::XMConvertToDegrees(euler.z);
	return euler;
}

const Vector3 MMMEngine::Transform::GetWorldScale() const
{
	if (m_parent)
	{
		Vector3 parentScale = m_parent->GetWorldScale();
		return Vector3{
			m_localScale.x * parentScale.x,
			m_localScale.y * parentScale.y,
			m_localScale.z * parentScale.z
		};
	}
	return m_localScale;
}

MMMEngine::ObjPtr<MMMEngine::Transform> MMMEngine::Transform::GetParent() const
{
	return m_parent;
}

MMMEngine::ObjPtr<MMMEngine::Transform> MMMEngine::Transform::GetChild(size_t index) const
{
	//out of index
	if (index >= m_childs.size())
		return ObjPtr<Transform>();

	return m_childs[index];
}

size_t MMMEngine::Transform::GetChildCount() const
{
	return m_childs.size();
}

void MMMEngine::Transform::SetWorldPosition(const Vector3& pos)
{
	if (!m_parent)
	{
		m_localPosition = pos;
	}
	else
	{
		const Vector3 pPos = m_parent->GetWorldPosition();
		const Quaternion pRot = m_parent->GetWorldRotation();
		const Vector3 pScale = m_parent->GetWorldScale();

		Vector3 v = pos - pPos;

		Quaternion invRot = pRot;
		invRot.Inverse(invRot); // 또는 pRot.Inverse(...)
		v = Vector3::Transform(v, invRot);

		const float eps = 1e-6f;
		m_localPosition = Vector3(
			(fabs(pScale.x) > eps) ? v.x / pScale.x : v.x,
			(fabs(pScale.y) > eps) ? v.y / pScale.y : v.y,
			(fabs(pScale.z) > eps) ? v.z / pScale.z : v.z
		);
	}

	MarkDirty();
	onMatrixUpdate.Invoke(this);
}

void MMMEngine::Transform::SetWorldRotation(const Quaternion& rot)
{
	if (m_parent)
	{
		Quaternion parentInvQuater = Quaternion::Identity;
		m_parent->GetWorldRotation().Inverse(parentInvQuater);
		m_localRotation = parentInvQuater * rot;
	}
	else
	{
		m_localRotation = rot;
	}

	MarkDirty();
	onMatrixUpdate.Invoke(this);
}

void MMMEngine::Transform::SetWorldEulerRotation(const Vector3& rot)
{
	// 오일러 각(도 단위)을 라디안 단위로 변환
	auto radRotX = DirectX::XMConvertToRadians(rot.x);
	auto radRotY = DirectX::XMConvertToRadians(rot.y);
	auto radRotZ = DirectX::XMConvertToRadians(rot.z);

	// 라디안 오일러 각으로 쿼터니언 생성
	Quaternion worldRot = Quaternion::CreateFromYawPitchRoll(radRotY, radRotX, radRotZ);

	SetWorldRotation(worldRot);
}

void MMMEngine::Transform::SetWorldScale(const Vector3& scale)
{
	if (m_parent)
	{
		Vector3 parentScale = m_parent->GetWorldScale();
		const float epsilon = 1e-6f;
		m_localScale = Vector3(
			(abs(parentScale.x) > epsilon) ? scale.x / parentScale.x : scale.x,
			(abs(parentScale.y) > epsilon) ? scale.y / parentScale.y : scale.y,
			(abs(parentScale.z) > epsilon) ? scale.z / parentScale.z : scale.z
		);
	}
	else
	{
		m_localScale = scale;
	}

	MarkDirty();
	onMatrixUpdate.Invoke(this);
}

void MMMEngine::Transform::SetLocalPosition(const Vector3& pos)
{
	m_localPosition = pos;
	MarkDirty();
	onMatrixUpdate.Invoke(this);
}

void MMMEngine::Transform::SetLocalRotation(const Quaternion& rot)
{
	m_localRotation = rot;
	MarkDirty();
	onMatrixUpdate.Invoke(this);
}

void MMMEngine::Transform::SetLocalScale(const Vector3& scale)
{
	m_localScale = scale;
	MarkDirty();
	onMatrixUpdate.Invoke(this);
}

void MMMEngine::Transform::SetParent(ObjPtr<Transform> parent, bool worldPositionStays)
{
	// 이미 같은 부모면 return
	if (parent == m_parent) return;

	for (auto p = parent; p != nullptr; p = p->m_parent)
	{
		if (p == SelfPtr(this))
			return; // cycle 거부
	}

	// 현재 월드 포지션 백업
	const auto worldScaleBefore = GetWorldScale();
	const auto worldRotationBefore = GetWorldRotation();
	const auto worldPositionBefore = GetWorldPosition();

	// 기존 부모에서 제거
	if (m_parent)
		m_parent->RemoveChild(SelfPtr(this));

	// 부모 교체
	m_parent = parent;

	if (m_parent)
		m_parent->AddChild(SelfPtr(this));

	if (worldPositionStays)
	{
		if (m_parent)
		{
			// 부모 기준 상 로컬 행렬
			auto pScale = m_parent->GetWorldScale();
			auto pRot = m_parent->GetWorldRotation();
			auto pPos = m_parent->GetWorldPosition();

			Quaternion invRot = pRot;
			invRot.Inverse(invRot);

			const float eps = 1e-6f;

			// scale
			m_localScale = Vector3(
				(fabs(pScale.x) > eps) ? worldScaleBefore.x / pScale.x : worldScaleBefore.x,
				(fabs(pScale.y) > eps) ? worldScaleBefore.y / pScale.y : worldScaleBefore.y,
				(fabs(pScale.z) > eps) ? worldScaleBefore.z / pScale.z : worldScaleBefore.z
			);


			m_localRotation = invRot * worldRotationBefore;
			m_localRotation.Normalize();

			Vector3 v = worldPositionBefore - pPos;
			v = Vector3::Transform(v, invRot);

			m_localPosition = Vector3(
				(fabs(pScale.x) > eps) ? v.x / pScale.x : v.x,
				(fabs(pScale.y) > eps) ? v.y / pScale.y : v.y,
				(fabs(pScale.z) > eps) ? v.z / pScale.z : v.z
			);
		}
		else
		{
			// 부모가 없으면 로컬 = 월드
			m_localScale = worldScaleBefore;
			m_localRotation = worldRotationBefore;
			m_localPosition = worldPositionBefore;
		}
	}

	MarkDirty();
	onDetachFromParent.Invoke(this);
	onMatrixUpdate.Invoke(this);  
	if(GetGameObject().IsValid())
		GetGameObject()->UpdateActiveInHierarchy(); // 부모가 바뀌었으므로 Hierarchy 활성화 상태 업데이트
}

MMMEngine::ObjPtr<MMMEngine::Transform> MMMEngine::Transform::Find(const std::string& path)
{
	if (path.empty())
		return nullptr;

	// 1. 경로를 '/'로 분할
	std::vector<std::string> tokens = StringHelper::Split(path, '/');
	if (tokens.empty())
		return nullptr;

	// 2. 현재 Transform에서 시작
	auto current = SelfPtr(this);

	// 3. 토큰 순회
	for (const auto& token : tokens)
	{
		bool found = false;

		for (const auto& child : current->m_childs)
		{
			if (child && child->GetName() == token)
			{
				// 일치하는 자식 발견
				current = child;
				found = true;
				break;
			}
		}

		if (!found)
		{
			// 현재 레벨에서 이름을 못 찾으면 경로 실패
			return ObjPtr<Transform>();
		}
	}

	return current;
}

MMMEngine::ObjPtr<MMMEngine::Transform> MMMEngine::Transform::GetRoot()
{
	auto current = SelfPtr(this);

	while (current->m_parent != nullptr)
	{
		if (!current.IsValid() || current->IsDestroyed())
			break;
		current = current->m_parent;
	}

	return current;
}

void MMMEngine::Transform::DetachChildren()
{
	std::vector<ObjPtr<Transform>> childrenCopy = m_childs;

	for (auto& child : childrenCopy)
	{
		if(child.IsValid() && !child->IsDestroyed())
			child->SetParent(nullptr);
	}

	m_childs.clear();
}

void MMMEngine::Transform::SetSiblingIndex(size_t idx)
{
	if (!m_parent)
		return;

	if (idx >= m_parent->m_childs.size())
	{
		// 인덱스가 범위를 벗어나면 아무 작업도 하지 않음
		return;
	}

	if (m_parent->m_childs[idx] == SelfPtr(this))
	{
		// 이미 해당 인덱스에 있다면 아무 작업도 하지 않음
		return;
	}

	//자식의 배열자 위치를 찾음
	auto& children = m_parent->m_childs;
	auto current_it = std::find(children.begin(), children.end(), this);

	// 현재 인덱스 계산
	size_t current_idx = std::distance(children.begin(), current_it);

	//std::rotate를 사용하여 요소 이동
	if (current_idx < idx)
	{
		// 현재 인덱스가 목표 인덱스보다 작으면 오른쪽으로 이동
		std::rotate(children.begin() + current_idx,
			children.begin() + current_idx + 1,
			children.begin() + idx + 1);
	}
	else
	{
		// 현재 인덱스가 목표 인덱스보다 크면 왼쪽으로 이동
		std::rotate(children.begin() + idx,
			children.begin() + current_idx,
			children.begin() + current_idx + 1);
	}
}

size_t MMMEngine::Transform::GetSiblingIndex() const
{
	if (m_parent)
	{
		auto it = std::find(m_parent->m_childs.begin(), m_parent->m_childs.end(), this);
		if (it != m_parent->m_childs.end())
		{
			return std::distance(m_parent->m_childs.begin(), it);
		}
	}
	return 0; // 부모가 없거나 찾을 수 없는 경우 0 반환
}

