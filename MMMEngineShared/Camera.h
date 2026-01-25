#pragma once
#include "Behaviour.h"

namespace MMMEngine
{
	class Camera : public Behaviour
	{
	private:
		float m_fov; // 0 ~ 360 degree
		float m_near; // near plane
		float m_far; // far plane
		float m_aspect; // { width / height } ratio
	};
}