#pragma once
#include <cstdint>
#include <array>
#include <physx/PxPhysicsAPI.h>

namespace MMMEngine
{
	class CollisionMatrix
	{
	public:
		static constexpr uint32_t kMaxLayers = 32;

		void ResetAll(bool collide = true)
		{
			for (uint32_t i = 0; i < kMaxLayers; ++i)
			{
				m_CollideMask[i] = collide ? 0xFFFFFFFFu : 0u;
			}
		}

		// A와 B의 충돌 허용을 설정(대칭)
		void SetCanCollide(uint32_t a, uint32_t b, bool can)
		{
			if (a >= kMaxLayers || b >= kMaxLayers) return;

			const uint32_t bitA = (1u << a);
			const uint32_t bitB = (1u << b);

			if (can)
			{
				m_CollideMask[a] |= bitB;
				m_CollideMask[b] |= bitA;
			}
			else
			{
				m_CollideMask[a] &= ~bitB;
				m_CollideMask[b] &= ~bitA;
			}
		}

		//2개의 레이어가 충돌허용인지 확인하는 함수
		bool CanCollide(uint32_t a, uint32_t b) const
		{
			if (a >= kMaxLayers || b >= kMaxLayers) return false;
			return (m_CollideMask[a] & (1u << b)) != 0;
		}

		//레이어 충돌 정책을 physx가 이해할수 있는 형태로 변환해주는 함수
		// Simulation(접촉/반작용/트리거 pair 결정용) filter data 생성
		// word0: 내 레이어 비트
		// word1: 내가 허용하는 상대 레이어 마스크
		physx::PxFilterData MakeSimFilter(uint32_t layer) const
		{
			physx::PxFilterData fd{};
			if (layer >= kMaxLayers) return fd;

			fd.word0 = (1u << layer);       //전달받은 레이어
			fd.word1 = m_CollideMask[layer]; //전달받은 레이어와 충돌이 허용된 레이어
			return fd;                       // return한 fd는 PxShape::setSimulationFilterData(fd)에서 사용한다
		}

		// Query(레이캐스트/스윕) filter data는 엔진 정책에 따라 다르게 쓰는데, (레이캐스트 / 스윕 / 오버랩(Query) 용도임)
		// 가장 단순한 버전은 sim과 동일한 방식으로 두어도 된다.
		physx::PxFilterData MakeQueryFilter(uint32_t layer) const
		{
			physx::PxFilterData fd{};
			if (layer >= kMaxLayers) return fd;

			fd.word0 = (1u << layer);
			fd.word1 = m_CollideMask[layer];
			return fd;
		}

	private:
		std::array<uint32_t, kMaxLayers> m_CollideMask{}; // layer -> allowed mask
	};
}