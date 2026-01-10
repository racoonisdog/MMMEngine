#include <iostream>

#define NOMINMAX

#include "GlobalRegistry.h"
#include "MMMApplication.h"
#include "App.h"
#include "MUID.h"
#include "GameObject.h"
#include "MMMInput.h"

using namespace MMMEngine;
using namespace MMMEngine::Utility;

#include <rttr/registration>
static void f() { std::cout << "Hello World" << std::endl; }
using namespace rttr;
RTTR_REGISTRATION
{
	using namespace rttr;
	registration::method("f", &f);
}

void Update() 
{
	InputManager::Get().Update();
	if (Input::GetKey(KeyCode::A))
	{
		std::cout << "A!" << std::endl;
	}

	if (Input::GetKeyDown(KeyCode::Escape))
		Application::Quit();
}

void Render() {  }

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

	App app;

	MMMEngine::g_pApp = &app;

	app.OnIntialize.AddListenerLambda([&app]() { InputManager::Get().StartUp(app.GetWindowHandle()); });
	app.OnUpdate.AddListener<&Update>();
	app.OnRender.AddListener<&Render>();

	act.Invoke();

	app.Run();

	MUID id = MUID::NewMUID();
	
	type::invoke("f", {});

	std::cout << "Generated MUID: " << id.ToString() << std::endl;
	auto FooObj = Object::NewObject<FooObject>();
	FooObj->Foo();

	auto go = Object::NewObject<GameObject>();
	auto ptrType = "ObjPtr<" + (*go).get_type().get_name().to_string() + ">";

	type class_type = type::get_by_name(ptrType);
	if (class_type)
	{
		auto var = class_type.create({ std::string{"overload"} });
		auto obj = var.get_value<ObjPtr<GameObject>>();
		std::cout << obj->GetName() << std::endl;

		std::cout << (*obj).GetMUID() << std::endl;

		auto prop = (*obj).get_type().get_property("Components");
		if (prop)
		{
			auto ss = prop.get_value(*obj);
			auto ssd = ss.get_value<std::vector<ObjPtr<Component>>>();

			auto size1 = ssd.size();

			std::cout << size1 << std::endl;
		}

		Object::Destroy(obj);

		if (!obj->IsDestroyed())
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