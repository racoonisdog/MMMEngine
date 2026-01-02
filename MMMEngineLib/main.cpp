#include <iostream>

#define NOMINMAX
#include "App.h"
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

class FooObject : public Object
{
public:
	void Foo()
	{
		auto inst = CreateInstance<GameObject>();
	}
};

int main()
{
	App app;

	Test testObj;

	Action<> act;
	act.AddListener<&Render>();

	act.RemoveListener<&Render>();

	act.AddListener<Test, &Test::Foo>(&testObj);

	app.onRender.AddListener<&Render>();

	act.Invoke();

	app.Run();

	MMMEngine::GUID id = MMMEngine::GUID::NewGuid();
	
	type::invoke("f", {});

	std::cout << "Generated GUID: " << id.ToString() << std::endl;



	return 0;
}