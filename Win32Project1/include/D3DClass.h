#ifndef __D3DClass_H__
#define __D3DClass_H__

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

#include <d3d12.h>
#include <dxgi1_4.h>

class D3DClass
{
public:
	D3DClass();
	D3DClass(const D3DClass&);
	~D3DClass();

	bool initialize(int, int, HWND, bool, bool);
	void shutdown();

	bool render();

private:
	bool m_vsync_enabled;
	ID3D12Device* m_device;
	ID3D12CommandQueue* m_commandQueue;
	char m_videoCardDescription[128];
	IDXGISwapChain3* m_swapChain;
	ID3D12DescriptorHeap* m_renderTargetViewHeap;
	ID3D12Resource* m_backBufferRenderTarget[2];
	unsigned int m_bufferIndex;
	ID3D12CommandAllocator* m_commandAllocator;
	ID3D12GraphicsCommandList* m_commandList;
	ID3D12PipelineState* m_pipelineState;
	ID3D12Fence* m_fence;
	HANDLE m_fenceEvent;
	unsigned long long m_fenceValue;
};

#endif //__D3DClass_H__