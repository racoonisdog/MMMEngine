#include <iostream>

#define NOMINMAX
#include "Application.h"
#include "GUID.h"
#include "GameObject.h"

using namespace MMMEngine;

#include <rttr/registration>
static void f() { std::cout << "Hello World" << std::endl; }
using namespace rttr;
RTTR_REGISTRATION
{
	using namespace rttr;
	registration::method("f", &f);
}

void Render() { std::cout << "drawcall" << std::endl; }

class Test
{
public:
	void Foo() { std::cout << "Foo called" << std::endl; }
};

class RollObject;
class FooObject : public Object
{
public:
	ObjPtr<RollObject> pRollObj;
	void Foo()
	{
		pRollObj = FindObjectByType<RollObject>();

		auto inst = NewObject<GameObject>();
		if (inst)
		{
			std::cout << GetName() << std::endl;
			std::cout << inst->GetName() << std::endl;
		}
	}
};

class RollObject : public Object
{
public:
	void A()
	{
		auto FooPtr = FindObjectByType<FooObject>();
		FooPtr->pRollObj = SelfPtr(this);
		FooPtr->pRollObj->SetName("watashiga tenni tatsu");
	}
};

int main()
{
	Test testObj;

	Action<> act;
	act.AddListener<&Render>();

	act.RemoveListener<&Render>();

	act.AddListener<Test, &Test::Foo>(&testObj);

	Application::Get().OnRender.AddListener<&Render>();

	act.Invoke();

	Application::Get().Run();

	MMMEngine::GUID id = MMMEngine::GUID::NewGuid();
	
	type::invoke("f", {});

	std::cout << "Generated GUID: " << id.ToString() << std::endl;
	auto FooObj = Object::NewObject<FooObject>();
	FooObj->Foo();

	auto go = Object::NewObject<GameObject>();
	auto ptrType = "ObjPtr<" + (*go).get_type().get_name().to_string() + ">";

	type class_type = type::get_by_name(ptrType);
	if (class_type)
	{
		auto obj = class_type.create({ std::string{"overload"} }).get_value<ObjPtr<GameObject>>();
		std::cout << obj->GetName() << std::endl;

		std::cout << (*obj).GetGUID() << std::endl;

		Object::Destroy(obj);

		if (obj)
			std::cout << "Alived!" << std::endl;
		else
			std::cout << "Destroyed!" << std::endl;
	}

	auto s = go.get_type().get_name().to_string();

	auto a = Object::NewObject<RollObject>();
	if(a) // È¤Àº a.IsValid()
		a->A();
	auto s2 = a.Cast<Object>()->GetName();
	auto s3 = a.As<Object>()->GetName();


	auto founds = Object::FindObjectsByType<Object>();

	for (auto& f : founds)
	{
		std::cout << f->GetName() << std::endl;
	}

	return 0;
}