#include "SystemClass.h"

SystemClass::SystemClass()
{
	input = 0;
	graphics = 0;
}

SystemClass::SystemClass(const SystemClass& other)
{
}


SystemClass::~SystemClass()
{
}

bool SystemClass::initialize()
{
	int screenHeight, screenWidth;
	bool result;
	
	screenHeight = 0;
	screenWidth = 0;

	// Initialize windows api
	initializeWindows(screenHeight, screenWidth);

	// Generate the input object to read keyboard input from the user
	input = new InputClass;
	if (!input)
	{
		return false;
	}

	input->initialize();

	// Create the graphics object that will handle rendering
	graphics = new GraphicsClass;
	if (!graphics)
	{
		return false;
	}

	result = graphics->initialize(screenHeight, screenWidth, m_hwnd);
	if (!result)
	{
		return false;
	}

	return true;
}

void SystemClass::shutDown()
{
	if (graphics)
	{
		graphics->shutDown();
		delete graphics;
		graphics = 0;
	}

	if (input)
	{
		delete input;
		input = 0;
	}

	shutdownWindows();
}

void SystemClass::run()
{
	MSG msg;
	bool done, result;

	// Initialize message
	ZeroMemory(&msg, sizeof(MSG));

	// Continue loop until exit message is received
	done = false;
	while (!done)
	{
		// Handle windows messages
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		// If windows signals to end the application then exit
		if (msg.message == WM_QUIT)
		{
			done = true;
		}
		else
		{
			// Process the frame
			result = frame();
			if (!result)
			{
				done = true;
			}
		}
	}
}

bool SystemClass::frame()
{
	bool result;

	// Check if the user pressed escape
	if (input->isKeyDown(VK_ESCAPE))
	{
		return false;
	}

	// Perform the frame processing for the graphics object
	result = graphics->frame();
	if (!result)
	{
		return false;
	}

	return true;
}

LRESULT CALLBACK SystemClass::MessageHandler(HWND hwnd, UINT umsg, WPARAM wparam, LPARAM lparam)
{
	switch (umsg)
	{
		// Handle key presses
		case WM_KEYDOWN:
		{
			input->keyDown((unsigned int)wparam);
			return 0;
		}

		// Handle keys being released
		case WM_KEYUP:
		{
			input->keyUp((unsigned int)wparam);
			return 0;
		}

		// All other messages should be handled by windows
		default:
		{
			return DefWindowProc(hwnd, umsg, wparam, lparam);
		}
	}
}

void SystemClass::initializeWindows(int &screenHeight, int& screenWidth)
{
	WNDCLASSEX wc;
	DEVMODE dmScreenSettings;
	int posX, posY;

	// Get an external pointer to this object
	ApplicationHandle = this;

	// Get the instance of this application
	m_hinstance = GetModuleHandle(NULL);

	// Give the application a name
	m_applicationName = L"DirectXEngine";

	// Setup the windows class with default settings
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = m_hinstance;
	wc.hIcon = LoadIcon(NULL, IDI_WINLOGO);
	wc.hIconSm = wc.hIcon;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = m_applicationName;
	wc.cbSize = sizeof(WNDCLASSEX);

	// Register the window class
	RegisterClassEx(&wc);

	// Determine the resolution of the clients desktop
	screenHeight = GetSystemMetrics(SM_CYSCREEN);
	screenWidth = GetSystemMetrics(SM_CXSCREEN);

	// Set up the screen settings depending on whether we are running in full screen or windowed mode
	if (FULL_SCREEN)
	{
		memset(&dmScreenSettings, 0, sizeof(dmScreenSettings));
		dmScreenSettings.dmSize = sizeof(dmScreenSettings);
		dmScreenSettings.dmPelsHeight = (unsigned long)screenHeight;
		dmScreenSettings.dmPelsWidth = (unsigned long)screenWidth;
		dmScreenSettings.dmBitsPerPel = 32;
		dmScreenSettings.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

		// Change the display settings to full screen
		ChangeDisplaySettings(&dmScreenSettings, CDS_FULLSCREEN);

		// Set the position of the window to the top left corner
		posX = posY = 0;
	}
	else
	{
		// If windowed then set the resolution to the predefined values
		screenWidth = WINDOWED_SCREEN_WIDTH;
		screenHeight = WINDOWED_SCREEN_HEIGHT;

		posX = (GetSystemMetrics(SM_CXSCREEN) - screenWidth) / 2;
		posY = (GetSystemMetrics(SM_CYSCREEN) - screenHeight) / 2;
	}

	// Create the window with the screen settings and save the handle
	m_hwnd = CreateWindowEx(WS_EX_APPWINDOW, m_applicationName, m_applicationName,
		WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_POPUP,
		posX, posY, screenWidth, screenHeight, NULL, NULL, m_hinstance, NULL);

	// Bring the window up on the screen and set it as the focus
	ShowWindow(m_hwnd, SW_SHOW);
	SetForegroundWindow(m_hwnd);
	SetFocus(m_hwnd);

	// Hide the mouse cursor
	ShowCursor(false);
}

void SystemClass::shutdownWindows()
{
	// Show the mouse cursor
	ShowCursor(true);

	// Fix the display settings if leaving full screen mode
	if (FULL_SCREEN)
	{
		ChangeDisplaySettings(NULL, 0);
	}

	// Remove the window
	DestroyWindow(m_hwnd);
	m_hwnd = NULL;

	// Remove the application instance
	UnregisterClass(m_applicationName, m_hinstance);
	m_hinstance = NULL;

	// Release the pointer to this class
	ApplicationHandle = NULL;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT umessage, WPARAM wparam, LPARAM lparam)
{
	switch (umessage)
	{
		// Check if the window is being destroyed
		case WM_DESTROY:
		{
			PostQuitMessage(0);
			return 0;
		}

		// Check if the window is being closed
		case WM_CLOSE:
		{
			PostQuitMessage(0);
			return 0;
		}

		// All other messages pass to the message handler in the system class
		default:
		{
			return ApplicationHandle->MessageHandler(hwnd, umessage, wparam, lparam);
		}
	}
}