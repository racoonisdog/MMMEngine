#define NOMINMAX
#include <DirectXMath.h>
#include <rttr/registration>

RTTR_REGISTRATION
{
	using namespace DirectX;
	using namespace rttr;

	registration::class_<XMFLOAT2>("XMFLOAT2")
		.constructor<>()
		.constructor<float, float>()
		.property("x", &XMFLOAT2::x)
		.property("y", &XMFLOAT2::y);

	registration::class_<XMFLOAT3>("XMFLOAT3")
		.constructor<>()
		.constructor<float, float, float>()
		.property("x", &XMFLOAT3::x)
		.property("y", &XMFLOAT3::y)
		.property("z", &XMFLOAT3::z);

	registration::class_<XMFLOAT4>("XMFLOAT4")
		.constructor<>()
		.constructor<float, float, float, float>()
		.property("x", &XMFLOAT4::x)
		.property("y", &XMFLOAT4::y)
		.property("z", &XMFLOAT4::z)
		.property("w", &XMFLOAT4::w);

	registration::class_<XMFLOAT4X4>("XMFLOAT4X4")
		.constructor<>()
		.constructor<float, float, float, float,
					float, float, float, float,
					float, float, float, float,
					float, float, float, float>()
		.property("_11", &XMFLOAT4X4::_11)
		.property("_12", &XMFLOAT4X4::_12)
		.property("_13", &XMFLOAT4X4::_13)
		.property("_14", &XMFLOAT4X4::_14)
		.property("_21", &XMFLOAT4X4::_21)
		.property("_22", &XMFLOAT4X4::_22)
		.property("_23", &XMFLOAT4X4::_23)
		.property("_24", &XMFLOAT4X4::_24)
		.property("_31", &XMFLOAT4X4::_31)
		.property("_32", &XMFLOAT4X4::_32)
		.property("_33", &XMFLOAT4X4::_33)
		.property("_34", &XMFLOAT4X4::_34)
		.property("_41", &XMFLOAT4X4::_41)
		.property("_42", &XMFLOAT4X4::_42)
		.property("_43", &XMFLOAT4X4::_43)
		.property("_44", &XMFLOAT4X4::_44);
}