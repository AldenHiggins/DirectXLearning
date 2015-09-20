#ifndef __GRAPHICSCLASS_H_
#define __GRAPHICSCLASS_H_

#include "D3DClass.h"

const bool FULL_SCREEN = true;
const bool VSYNC_ENABLED = true;
const float SCREEN_DEPTH = 1000.0f;
const float SCREEN_NEAR = 0.1f;
const int WINDOWED_SCREEN_WIDTH = 800;
const int WINDOWED_SCREEN_HEIGHT = 600;

class GraphicsClass
{
public:
	GraphicsClass();
	GraphicsClass(const GraphicsClass&);
	~GraphicsClass();

	bool initialize(int, int, HWND);
	void shutDown();
	bool frame();

private:
	bool render();

private:
	D3DClass *direct3D;
};


#endif //__GRAPHICSCLASS_H_
