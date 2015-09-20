#include "GraphicsClass.h"

GraphicsClass::GraphicsClass()
{
	direct3D = 0;
}


GraphicsClass::GraphicsClass(const GraphicsClass& other)
{
}


GraphicsClass::~GraphicsClass()
{
}


bool GraphicsClass::initialize(int screenHeight, int screenWidth, HWND hwnd)
{
	bool result;

	// Create the Direct3D object
	direct3D = new D3DClass;
	if (!direct3D)
	{
		return false;
	}

	return true;
}


void GraphicsClass::shutDown()
{

	return;
}


bool GraphicsClass::frame()
{

	return true;
}


bool GraphicsClass::render()
{

	return true;
}
