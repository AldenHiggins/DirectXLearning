﻿#pragma once

#include "..\Common\DeviceResources.h"
#include "ShaderStructures.h"
#include "..\Common\StepTimer.h"
#include "SimpleCamera.h"

namespace DirectX12Engine
{
	// This sample renderer instantiates a basic rendering pipeline.
	class Sample3DSceneRenderer
	{
	public:
		Sample3DSceneRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources);
		~Sample3DSceneRenderer();
		void CreateDeviceDependentResources();
		void CreateWindowSizeDependentResources();
		void Update(DX::StepTimer const& timer);
		bool Render();
		void SaveState();

		void StartTracking();
		void TrackingUpdate(float positionX);
		void StopTracking();
		bool IsTracking() { return m_tracking; }
		void KeyEvent(Windows::UI::Core::KeyEventArgs^ args);
		void KeyUpEvent(Windows::UI::Core::KeyEventArgs^ args);

	private:
		void LoadState();
		void Rotate(float radians);

	private:
		// Constant buffers must be 256-byte aligned.
		static const UINT c_alignedConstantBufferSize = (sizeof(ModelViewProjectionConstantBuffer) + 255) & ~255;

		// Cached pointer to device resources.
		std::shared_ptr<DX::DeviceResources> m_deviceResources;

		// Direct3D resources for cube geometry.
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>	m_commandList;
		Microsoft::WRL::ComPtr<ID3D12RootSignature>			m_rootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState>			m_pipelineState;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		m_cbvHeap;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		m_srvHeap;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		m_samplerHeap;
		Microsoft::WRL::ComPtr<ID3D12Resource>				m_vertexBuffer;
		Microsoft::WRL::ComPtr<ID3D12Resource>				m_indexBuffer;
		Microsoft::WRL::ComPtr<ID3D12Resource>				m_constantBuffer;
		Microsoft::WRL::ComPtr<ID3D12Resource>				m_texture;
		ModelViewProjectionConstantBuffer					m_constantBufferData;
		UINT8*												m_mappedConstantBuffer;
		UINT												m_cbvDescriptorSize;
		D3D12_RECT											m_scissorRect;
		std::vector<byte>									m_vertexShader;
		std::vector<byte>									m_pixelShader;
		D3D12_VERTEX_BUFFER_VIEW							m_vertexBufferView;
		D3D12_INDEX_BUFFER_VIEW								m_indexBufferView;

		// Camera variables
		SimpleCamera m_camera;

		float m_cameraPitch;
		float m_cameraYaw;

		// Position variables for the box
		float boxHeight;

		// Variables used with the rendering loop.
		bool	m_loadingComplete;
		float	m_radiansPerSecond;
		bool	m_tracking;

		// Temporary texture variables and generation functions
		static const UINT TextureWidth = 256;
		static const UINT TextureHeight = 256;
		static const UINT TexturePixelSize = 4;
		std::vector<UINT8> GenerateTextureData();

	};
}

