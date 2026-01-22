#define NOMINMAX
#include "SimpleMath.h"
#include <rttr/registration>

RTTR_REGISTRATION
{
	using namespace DirectX::SimpleMath;
	using namespace rttr;

	registration::class_<DirectX::SimpleMath::Vector2>("Vector2")
		.constructor<>()
		.constructor<float, float>()
		.property("x", &Vector2::x)
		.property("y", &Vector2::y);

	registration::class_<DirectX::SimpleMath::Vector3>("Vector3")
		.constructor<>()
		.constructor<float, float, float>()
		.property("x", &Vector3::x)
		.property("y", &Vector3::y)
		.property("z", &Vector3::z);

	registration::class_<DirectX::SimpleMath::Vector4>("Vector4")
		.constructor<>()
		.constructor<float, float, float, float>()
		.property("x", &Vector4::x)
		.property("y", &Vector4::y)
		.property("z", &Vector4::z)
		.property("w", &Vector4::w);

	registration::class_<DirectX::SimpleMath::Quaternion>("Quaternion")
		.constructor<>()
		.constructor<float, float, float, float>()
		.property("x", &Quaternion::x)
		.property("y", &Quaternion::y)
		.property("z", &Quaternion::z)
		.property("w", &Quaternion::w);

	registration::class_<DirectX::SimpleMath::Color>("Color")
		.constructor<>()
		.constructor<float, float, float, float>()
		.property("x", &Color::x)
		.property("y", &Color::y)
		.property("z", &Color::z)
		.property("w", &Color::w);

	registration::class_<DirectX::SimpleMath::Matrix>("Matrix")
		.constructor<>()
		.constructor<float, float, float, float,
					float, float, float, float, 
					float, float, float, float, 
					float, float, float, float>()
		.property("_11", &Matrix::_11)
		.property("_12", &Matrix::_12)
		.property("_13", &Matrix::_13)
		.property("_14", &Matrix::_14)
		.property("_21", &Matrix::_21)
		.property("_22", &Matrix::_22)
		.property("_23", &Matrix::_23)
		.property("_24", &Matrix::_24)
		.property("_31", &Matrix::_31)
		.property("_32", &Matrix::_32)
		.property("_33", &Matrix::_33)
		.property("_34", &Matrix::_34)
		.property("_41", &Matrix::_41)
		.property("_42", &Matrix::_42)
		.property("_43", &Matrix::_43)
		.property("_44", &Matrix::_44);
}