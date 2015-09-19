// Beginning to learn DirectX!
#include "SystemClass.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR pScmdLine, int iCmdShow)
{
	SystemClass *system;
	bool result;

	// Generate the system
	system = new SystemClass;
	if (!system)
	{
		return 0;
	}

	result = system->initialize();
	if (result)
	{
		system->run();
	}

	// Shutdown the system
	system->shutDown();
	delete system;
	system = 0;

	return 0;
}