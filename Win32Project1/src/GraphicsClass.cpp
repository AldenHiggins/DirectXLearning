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

	// Initialize the Direct3D object
	result = direct3D->initialize(screenHeight, screenWidth, hwnd, VSYNC_ENABLED, FULL_SCREEN);

	if (!result)
	{
		MessageBox(hwnd, L"Could not initialize Direct3D", L"Error", MB_OK);
		return false;
	}

	return true;
}


void GraphicsClass::shutDown()
{
	if (direct3D)
	{
		direct3D->shutdown();
		delete direct3D;
		direct3D = 0;
	}

	return;
}


bool GraphicsClass::frame()
{
	bool result = render();

	if (!result)
	{
		return false;
	}

	return true;
}


bool GraphicsClass::render()
{
	// Render the scene via the direct3D object
	bool result = direct3D->render();
	if (!result)
	{
		return false;
	}

	return true;
}
