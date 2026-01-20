#include "CollisionMatrix.h"

//void MMMEngine::CollisionMatrix::ResetAll(bool collide)
//{
//    for (uint32_t i = 0; i < kMaxLayers; ++i)
//    {
//        m_CollideMask[i] = collide ? 0xFFFFFFFFu : 0u;
//    }
//}
//
//void MMMEngine::CollisionMatrix::SetCanCollide(uint32_t a, uint32_t b, bool can)
//{
//	if (a >= kMaxLayers || b >= kMaxLayers) return;
//
//	const uint32_t bitA = (1u << a);
//	const uint32_t bitB = (1u << b);
//
//	if (can)
//	{
//		m_CollideMask[a] |= bitB;
//		m_CollideMask[b] |= bitA;
//	}
//	else
//	{
//		m_CollideMask[a] &= ~bitB;
//		m_CollideMask[b] &= ~bitA;
//	}
//}
//
//bool MMMEngine::CollisionMatrix::CanCollide(uint32_t a, uint32_t b) const
//{
//	if (a >= kMaxLayers || b >= kMaxLayers) return false;
//	return (m_CollideMask[a] & (1u << b)) != 0;
//}
