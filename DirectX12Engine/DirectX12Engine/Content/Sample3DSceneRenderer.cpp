﻿#include "pch.h"
#include "Sample3DSceneRenderer.h"

#include "..\Common\DirectXHelper.h"
#include <ppltasks.h>
#include <synchapi.h>

using namespace DirectX12Engine;

using namespace Concurrency;
using namespace DirectX;
using namespace Microsoft::WRL;
using namespace Windows::Foundation;
using namespace Windows::Storage;

// Indices into the application state map.
Platform::String^ AngleKey = "Angle";
Platform::String^ TrackingKey = "Tracking";

// Camera variables
DirectX::XMVECTORF32 eye;
DirectX::XMVECTORF32 at;
DirectX::XMVECTORF32 up;

DirectX::XMFLOAT3 rotatedVectorPrint;

// Loads vertex and pixel shaders from files and instantiates the cube geometry.
Sample3DSceneRenderer::Sample3DSceneRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
	m_loadingComplete(false),
	m_radiansPerSecond(XM_PIDIV4),	// rotate 45 degrees per second
	m_tracking(false),
	m_mappedConstantBuffer(nullptr),
	m_deviceResources(deviceResources)
{
	LoadState();
	ZeroMemory(&m_constantBufferData, sizeof(m_constantBufferData));

	m_cameraPitch = 0;
	m_cameraYaw = 0;
	boxHeight = 0;

	CreateDeviceDependentResources();
	CreateWindowSizeDependentResources();
}

Sample3DSceneRenderer::~Sample3DSceneRenderer()
{
	m_constantBuffer->Unmap(0, nullptr);
	m_mappedConstantBuffer = nullptr;
}

void Sample3DSceneRenderer::CreateDeviceDependentResources()
{
	auto d3dDevice = m_deviceResources->GetD3DDevice();

	// Create a root signature with a single constant buffer slot.
	{
		CD3DX12_DESCRIPTOR_RANGE range[2];
		CD3DX12_ROOT_PARAMETER parameter[2];
		
		range[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
		range[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
		parameter[0].InitAsDescriptorTable(1, &range[0], D3D12_SHADER_VISIBILITY_VERTEX);
		parameter[1].InitAsDescriptorTable(1, &range[1], D3D12_SHADER_VISIBILITY_PIXEL);

		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT; // Only the input assembler stage needs access to the constant buffer.
			//D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			//D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
			//D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			//D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

		D3D12_STATIC_SAMPLER_DESC sampler = {};
		sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler.MipLODBias = 0;
		sampler.MaxAnisotropy = 0;
		sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		sampler.MinLOD = 0.0f;
		sampler.MaxLOD = D3D12_FLOAT32_MAX;
		sampler.ShaderRegister = 0;
		sampler.RegisterSpace = 0;
		sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		CD3DX12_ROOT_SIGNATURE_DESC descRootSignature;
		descRootSignature.Init(_countof(parameter), parameter, 1, &sampler, rootSignatureFlags);
		//descRootSignature.Init(_countof(parameter), parameter, 0, nullptr);

		ComPtr<ID3DBlob> pSignature;
		ComPtr<ID3DBlob> pError;
		DX::ThrowIfFailed(D3D12SerializeRootSignature(&descRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, pSignature.GetAddressOf(), pError.GetAddressOf()));
		DX::ThrowIfFailed(d3dDevice->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
	}

	// Load shaders asynchronously.
	auto createVSTask = DX::ReadDataAsync(L"SampleVertexShader.cso").then([this](std::vector<byte>& fileData) {
		m_vertexShader = fileData;
	});

	auto createPSTask = DX::ReadDataAsync(L"SamplePixelShader.cso").then([this](std::vector<byte>& fileData) {
		m_pixelShader = fileData;
	});

	// Create the pipeline state once the shaders are loaded.
	auto createPipelineStateTask = (createPSTask && createVSTask).then([this]() {

		static const D3D12_INPUT_ELEMENT_DESC inputLayout[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		D3D12_GRAPHICS_PIPELINE_STATE_DESC state = {};
		state.InputLayout = { inputLayout, _countof(inputLayout) };
		state.pRootSignature = m_rootSignature.Get();
		state.VS = { &m_vertexShader[0], m_vertexShader.size() };
		state.PS = { &m_pixelShader[0], m_pixelShader.size() };
		state.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		state.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		state.DepthStencilState.DepthEnable = FALSE;
		state.DepthStencilState.StencilEnable = FALSE;
		state.SampleMask = UINT_MAX;
		state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		state.NumRenderTargets = 1;
		state.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
		state.SampleDesc.Count = 1;

		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&m_pipelineState)));

		// Shader data can be deleted once the pipeline state is created.
		m_vertexShader.clear();
		m_pixelShader.clear();
	});

	// Create and upload cube geometry resources to the GPU.
	auto createAssetsTask = createPipelineStateTask.then([this]() {
		auto d3dDevice = m_deviceResources->GetD3DDevice();

		// Create a command list.
		DX::ThrowIfFailed(d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_deviceResources->GetCommandAllocator(), m_pipelineState.Get(), IID_PPV_ARGS(&m_commandList)));

		// Cube vertices. Each vertex has a position and a color.
		VertexTextureCoordinate cubeVertices[] =
		{
			{ XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT2(0.0f, 0.0f) },
			{ XMFLOAT3(-0.5f, -0.5f,  0.5f), XMFLOAT2(0.0f, 0.0f) },
			{ XMFLOAT3(-0.5f,  0.5f, -0.5f), XMFLOAT2(0.0f, 1.0f) },
			{ XMFLOAT3(-0.5f,  0.5f,  0.5f), XMFLOAT2(0.0f, 1.0f) },
			{ XMFLOAT3(0.5f, -0.5f, -0.5f), XMFLOAT2(1.0f, 0.0f) },
			{ XMFLOAT3(0.5f, -0.5f,  0.5f), XMFLOAT2(1.0f, 0.0f) },
			{ XMFLOAT3(0.5f,  0.5f, -0.5f), XMFLOAT2(1.0f, 1.0f) },
			{ XMFLOAT3(0.5f,  0.5f,  0.5f), XMFLOAT2(1.0f, 1.0f) },

			// Floor vertices
			{ XMFLOAT3(-3.0f, 0.0f, -3.0f), XMFLOAT2(0.8f, 0.8f) },
			{ XMFLOAT3(-3.0f, 0.0f,  3.0f), XMFLOAT2(0.8f, 0.8f) },
			{ XMFLOAT3(3.0f,  0.0f, 3.0f), XMFLOAT2(0.8f, 0.8f) },
			{ XMFLOAT3(3.0f,  0.0f,  -3.0f), XMFLOAT2(0.8f, 0.8f) },
		};

		const UINT vertexBufferSize = sizeof(cubeVertices);

		// Create the vertex buffer resource in the GPU's default heap and copy vertex data into it using the upload heap.
		// The upload resource must not be released until after the GPU has finished using it.
		Microsoft::WRL::ComPtr<ID3D12Resource> vertexBufferUpload;

		CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
		CD3DX12_RESOURCE_DESC vertexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
		DX::ThrowIfFailed(d3dDevice->CreateCommittedResource(
			&defaultHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&vertexBufferDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&m_vertexBuffer)));

		CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
		DX::ThrowIfFailed(d3dDevice->CreateCommittedResource(
			&uploadHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&vertexBufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&vertexBufferUpload)));

		m_vertexBuffer->SetName(L"Vertex Buffer Resource");
		vertexBufferUpload->SetName(L"Vertex Buffer Upload Resource");

		// Upload the vertex buffer to the GPU.
		{
			D3D12_SUBRESOURCE_DATA vertexData = {};
			vertexData.pData = reinterpret_cast<BYTE*>(cubeVertices);
			vertexData.RowPitch = vertexBufferSize;
			vertexData.SlicePitch = vertexData.RowPitch;

			UpdateSubresources(m_commandList.Get(), m_vertexBuffer.Get(), vertexBufferUpload.Get(), 0, 0, 1, &vertexData);

			CD3DX12_RESOURCE_BARRIER vertexBufferResourceBarrier =
				CD3DX12_RESOURCE_BARRIER::Transition(m_vertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
			m_commandList->ResourceBarrier(1, &vertexBufferResourceBarrier);
		}

		// Load mesh indices. Each trio of indices represents a triangle to be rendered on the screen.
		// For example: 0,2,1 means that the vertices with indexes 0, 2 and 1 from the vertex buffer compose the
		// first triangle of this mesh.
		unsigned short cubeIndices[] =
		{
			0, 2, 1, // -x
			1, 2, 3,

			4, 5, 6, // +x
			5, 7, 6,

			0, 1, 5, // -y
			0, 5, 4,

			2, 6, 7, // +y
			2, 7, 3,

			0, 4, 6, // -z
			0, 6, 2,

			1, 3, 7, // +z
			1, 7, 5,

			// Floor indices
			11, 9, 8,
			10, 9, 11,
		};

		const UINT indexBufferSize = sizeof(cubeIndices);

		// Create the index buffer resource in the GPU's default heap and copy index data into it using the upload heap.
		// The upload resource must not be released until after the GPU has finished using it.
		Microsoft::WRL::ComPtr<ID3D12Resource> indexBufferUpload;

		CD3DX12_RESOURCE_DESC indexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);
		DX::ThrowIfFailed(d3dDevice->CreateCommittedResource(
			&defaultHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&indexBufferDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&m_indexBuffer)));

		DX::ThrowIfFailed(d3dDevice->CreateCommittedResource(
			&uploadHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&indexBufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&indexBufferUpload)));

		m_indexBuffer->SetName(L"Index Buffer Resource");
		indexBufferUpload->SetName(L"Index Buffer Upload Resource");

		// Upload the index buffer to the GPU.
		{
			D3D12_SUBRESOURCE_DATA indexData = {};
			indexData.pData = reinterpret_cast<BYTE*>(cubeIndices);
			indexData.RowPitch = indexBufferSize;
			indexData.SlicePitch = indexData.RowPitch;

			UpdateSubresources(m_commandList.Get(), m_indexBuffer.Get(), indexBufferUpload.Get(), 0, 0, 1, &indexData);

			CD3DX12_RESOURCE_BARRIER indexBufferResourceBarrier =
				CD3DX12_RESOURCE_BARRIER::Transition(m_indexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			m_commandList->ResourceBarrier(1, &indexBufferResourceBarrier);
		}

		// Create a descriptor heap for the constant buffers.
		{
			D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
			heapDesc.NumDescriptors = DX::c_frameCount + 1; // + 1 for the Shader resource view
			heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			// This flag indicates that this descriptor heap can be bound to the pipeline and that descriptors contained in it can be referenced by a root table.
			heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			DX::ThrowIfFailed(d3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_cbvHeap)));

			m_cbvHeap->SetName(L"Constant Buffer View Descriptor Heap");

			//// Describe and create a sampler descriptor heap.
			//D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDesc = {};
			//samplerHeapDesc.NumDescriptors = 1;
			//samplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
			//samplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			//DX::ThrowIfFailed(d3dDevice->CreateDescriptorHeap(&samplerHeapDesc, IID_PPV_ARGS(&m_samplerHeap)));

			//// Describe and create a shader resource view (SRV) heap for the texture.
			//D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
			//srvHeapDesc.NumDescriptors = 1;
			//srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			//srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			//DX::ThrowIfFailed(d3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvHeap)));
		}

		CD3DX12_RESOURCE_DESC constantBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(DX::c_frameCount * c_alignedConstantBufferSize);
		DX::ThrowIfFailed(d3dDevice->CreateCommittedResource(
			&uploadHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&constantBufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_constantBuffer)));

		m_constantBuffer->SetName(L"Constant Buffer");

		// Create constant buffer views to access the upload buffer.
		D3D12_GPU_VIRTUAL_ADDRESS cbvGpuAddress = m_constantBuffer->GetGPUVirtualAddress();
		CD3DX12_CPU_DESCRIPTOR_HANDLE cbvCpuHandle(m_cbvHeap->GetCPUDescriptorHandleForHeapStart());
		m_cbvDescriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		for (int n = 0; n < DX::c_frameCount; n++)
		{
			D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
			desc.BufferLocation = cbvGpuAddress;
			desc.SizeInBytes = c_alignedConstantBufferSize;
			d3dDevice->CreateConstantBufferView(&desc, cbvCpuHandle);

			cbvGpuAddress += desc.SizeInBytes;
			cbvCpuHandle.Offset(m_cbvDescriptorSize);
		}

		// Map the constant buffers.
		DX::ThrowIfFailed(m_constantBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_mappedConstantBuffer)));
		ZeroMemory(m_mappedConstantBuffer, DX::c_frameCount * c_alignedConstantBufferSize);
		// We don't unmap this until the app closes. Keeping things mapped for the lifetime of the resource is okay.

		// Close the command list and execute it to begin the vertex/index buffer copy into the GPU's default heap.
		DX::ThrowIfFailed(m_commandList->Close());
		ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
		m_deviceResources->GetCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

		// Create vertex/index buffer views.
		m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
		m_vertexBufferView.StrideInBytes = sizeof(VertexTextureCoordinate);
		m_vertexBufferView.SizeInBytes = sizeof(cubeVertices);

		m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
		m_indexBufferView.SizeInBytes = sizeof(cubeIndices);
		m_indexBufferView.Format = DXGI_FORMAT_R16_UINT;

		// Create an upload heap to load the texture onto the GPU. ComPtr's are CPU objects
		// but this heap needs to stay in scope until the GPU work is complete. We will
		// synchronize with the GPU at the end of this method before the ComPtr is destroyed.
		ComPtr<ID3D12Resource> textureUploadHeap;

		// Create the texture.
		{
			// Describe and create a Texture2D.
			D3D12_RESOURCE_DESC textureDesc = {};
			textureDesc.MipLevels = 1;
			textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			textureDesc.Width = TextureWidth;
			textureDesc.Height = TextureHeight;
			textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
			textureDesc.DepthOrArraySize = 1;
			textureDesc.SampleDesc.Count = 1;
			textureDesc.SampleDesc.Quality = 0;
			textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

			DX::ThrowIfFailed(d3dDevice->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&textureDesc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(&m_texture)));

			const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_texture.Get(), 0, 1);

			// Create the GPU upload buffer.
			DX::ThrowIfFailed(d3dDevice->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&textureUploadHeap)));

			// Copy data to the intermediate upload heap and then schedule a copy 
			// from the upload heap to the Texture2D.
			std::vector<UINT8> texture = GenerateTextureData();

			D3D12_SUBRESOURCE_DATA textureData = {};
			textureData.pData = &texture[0];
			textureData.RowPitch = TextureWidth * TexturePixelSize;
			textureData.SlicePitch = textureData.RowPitch * TextureHeight;

			UpdateSubresources(m_commandList.Get(), m_texture.Get(), textureUploadHeap.Get(), 0, 0, 1, &textureData);
			m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

			//// Describe and create a sampler.
			//D3D12_SAMPLER_DESC samplerDesc = {};
			//samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			//samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			//samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			//samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			//samplerDesc.MinLOD = 0;
			//samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
			//samplerDesc.MipLODBias = 0.0f;
			//samplerDesc.MaxAnisotropy = 1;
			//samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			//d3dDevice->CreateSampler(&samplerDesc, m_samplerHeap->GetCPUDescriptorHandleForHeapStart());

			// Describe and create a SRV for the texture.
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Format = textureDesc.Format;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = 1;
			CD3DX12_CPU_DESCRIPTOR_HANDLE cbvSrvHandle(m_cbvHeap->GetCPUDescriptorHandleForHeapStart(), 3, m_cbvDescriptorSize);
			d3dDevice->CreateShaderResourceView(m_texture.Get(), &srvDesc, cbvSrvHandle);
		}

		// Wait for the command list to finish executing; the vertex/index buffers need to be uploaded to the GPU before the upload resources go out of scope.
		m_deviceResources->WaitForGpu();
	});

	createAssetsTask.then([this]() {
		m_loadingComplete = true;
	});
}

// Initializes view parameters when the window size changes.
void Sample3DSceneRenderer::CreateWindowSizeDependentResources()
{
	Size outputSize = m_deviceResources->GetOutputSize();
	float aspectRatio = outputSize.Width / outputSize.Height;
	float fovAngleY = 70.0f * XM_PI / 180.0f;

	D3D12_VIEWPORT viewport = m_deviceResources->GetScreenViewport();
	m_scissorRect = { 0, 0, static_cast<LONG>(viewport.Width), static_cast<LONG>(viewport.Height)};

	// This is a simple example of change that can be made when the app is in
	// portrait or snapped view.
	if (aspectRatio < 1.0f)
	{
		fovAngleY *= 2.0f;
	}

	// Note that the OrientationTransform3D matrix is post-multiplied here
	// in order to correctly orient the scene to match the display orientation.
	// This post-multiplication step is required for any draw calls that are
	// made to the swap chain render target. For draw calls to other targets,
	// this transform should not be applied.

	// This sample makes use of a right-handed coordinate system using row-major matrices.
	XMMATRIX perspectiveMatrix = XMMatrixPerspectiveFovRH(
		fovAngleY,
		aspectRatio,
		0.01f,
		100.0f
		);

	XMFLOAT4X4 orientation = m_deviceResources->GetOrientationTransform3D();
	XMMATRIX orientationMatrix = XMLoadFloat4x4(&orientation);

	XMStoreFloat4x4(
		&m_constantBufferData.projection,
		XMMatrixTranspose(perspectiveMatrix * orientationMatrix)
		);

	// Eye is at (0,0.7,1.5), looking at point (0,-0.1,0) with the up-vector along the y-axis.
	eye = { 0.0f, 0.7f, 1.5f, 0.0f };
	at = { 0.0f, 0.0f, 0.0f, 0.0f };
	up = { 0.0f, 1.0f, 0.0f, 0.0f };

	XMStoreFloat4x4(&m_constantBufferData.view, XMMatrixTranspose(XMMatrixLookAtRH(eye, at, up)));
}

// Called once per frame, rotates the cube and calculates the model and view matrices.
void Sample3DSceneRenderer::Update(DX::StepTimer const& timer)
{
	if (m_loadingComplete)
	{
		if (!m_tracking)
		{
			// Rotate the cube a small amount.
			//m_angle += static_cast<float>(timer.GetElapsedSeconds()) * m_radiansPerSecond;

			Rotate(.01);

			XMVECTOR atVec = at;
			XMVECTOR eyeVec = eye;

			// Generate a new vector to determine where the camera should be looking based on user input
			XMVECTOR rotatedVector = XMVector3TransformCoord(atVec - eyeVec, XMMatrixRotationRollPitchYaw(m_cameraPitch, m_cameraYaw, 0.0f));

			XMStoreFloat4x4(&m_constantBufferData.view, XMMatrixTranspose(
				XMMatrixLookAtRH(eye, eyeVec + rotatedVector, up)));

			//XMStoreFloat4x4(&m_constantBufferData.model, XMMatrixTranspose(XMMatrixTranslation(0.0f, boxHeight, 0.0f)));

		}

		// Update the constant buffer resource.
		UINT8* destination = m_mappedConstantBuffer + (m_deviceResources->GetCurrentFrameIndex() * c_alignedConstantBufferSize);
		memcpy(destination, &m_constantBufferData, sizeof(m_constantBufferData));
	}
}

// Saves the current state of the renderer.
void Sample3DSceneRenderer::SaveState()
{
	auto state = ApplicationData::Current->LocalSettings->Values;

	if (state->HasKey(AngleKey))
	{
		state->Remove(AngleKey);
	}
	if (state->HasKey(TrackingKey))
	{
		state->Remove(TrackingKey);
	}

	//state->Insert(AngleKey, PropertyValue::CreateSingle(m_angle));
	state->Insert(TrackingKey, PropertyValue::CreateBoolean(m_tracking));
}

// Restores the previous state of the renderer.
void Sample3DSceneRenderer::LoadState()
{
	auto state = ApplicationData::Current->LocalSettings->Values;
	if (state->HasKey(AngleKey))
	{
		//m_angle = safe_cast<IPropertyValue^>(state->Lookup(AngleKey))->GetSingle();
		state->Remove(AngleKey);
	}
	if (state->HasKey(TrackingKey))
	{
		m_tracking = safe_cast<IPropertyValue^>(state->Lookup(TrackingKey))->GetBoolean();
		state->Remove(TrackingKey);
	}
}

// Rotate the 3D cube model a set amount of radians.
void Sample3DSceneRenderer::Rotate(float radians)
{
	// Prepare to pass the updated model matrix to the shader.
	XMStoreFloat4x4(&m_constantBufferData.model, XMMatrixTranspose(XMMatrixRotationY(radians) * XMMatrixTranslation(0.0f, 0.5f, 0.0f)));
}

void Sample3DSceneRenderer::StartTracking()
{
	m_tracking = true;
}

// When tracking, the 3D cube can be rotated around its Y axis by tracking pointer position relative to the output screen width.
void Sample3DSceneRenderer::TrackingUpdate(float positionX)
{
	if (m_tracking)
	{
		float radians = XM_2PI * 2.0f * positionX / m_deviceResources->GetOutputSize().Width;
		Rotate(radians);
	}
}

void Sample3DSceneRenderer::StopTracking()
{
	m_tracking = false;
}

// Renders one frame using the vertex and pixel shaders.
bool Sample3DSceneRenderer::Render()
{
	// Loading is asynchronous. Only draw geometry after it's loaded.
	if (!m_loadingComplete)
	{
		return false;
	}

	DX::ThrowIfFailed(m_deviceResources->GetCommandAllocator()->Reset());

	// The command list can be reset anytime after ExecuteCommandList() is called.
	DX::ThrowIfFailed(m_commandList->Reset(m_deviceResources->GetCommandAllocator(), m_pipelineState.Get()));

	PIXBeginEvent(m_commandList.Get(), 0, L"Draw the cube");
	{
		// Set the graphics root signature and descriptor heaps to be used by this frame.
		m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
		ID3D12DescriptorHeap* ppHeaps[] = { m_cbvHeap.Get() };
		m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

		// Bind the current frame's constant buffer to the pipeline.
		CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(m_cbvHeap->GetGPUDescriptorHandleForHeapStart(), m_deviceResources->GetCurrentFrameIndex(), m_cbvDescriptorSize);
		m_commandList->SetGraphicsRootDescriptorTable(0, gpuHandle);
		CD3DX12_GPU_DESCRIPTOR_HANDLE srvGpuHandle(m_cbvHeap->GetGPUDescriptorHandleForHeapStart(), 3, m_cbvDescriptorSize);
		m_commandList->SetGraphicsRootDescriptorTable(1, srvGpuHandle);
		//m_commandList->SetGraphicsRootDescriptorTable(1, m_samplerHeap->GetGPUDescriptorHandleForHeapStart());

		// Set the viewport and scissor rectangle.
		D3D12_VIEWPORT viewport = m_deviceResources->GetScreenViewport();
		m_commandList->RSSetViewports(1, &viewport);
		m_commandList->RSSetScissorRects(1, &m_scissorRect);

		// Indicate this resource will be in use as a render target.
		CD3DX12_RESOURCE_BARRIER renderTargetResourceBarrier =
			CD3DX12_RESOURCE_BARRIER::Transition(m_deviceResources->GetRenderTarget(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_commandList->ResourceBarrier(1, &renderTargetResourceBarrier);

		// Record drawing commands.
		m_commandList->ClearRenderTargetView(m_deviceResources->GetRenderTargetView(), DirectX::Colors::CornflowerBlue, 0, nullptr);
		D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView = m_deviceResources->GetRenderTargetView();
		m_commandList->OMSetRenderTargets(1, &renderTargetView, false, nullptr);

		m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
		m_commandList->IASetIndexBuffer(&m_indexBufferView);

		m_commandList->DrawIndexedInstanced(6, 1, 36, 0, 0);

		// Switch the model matrix to draw the ground in the right spot
		XMStoreFloat4x4(&m_constantBufferData.model, XMMatrixIdentity());
		m_commandList->DrawIndexedInstanced(36, 1, 0, 0, 0);

		// Indicate that the render target will now be used to present when the command list is done executing.
		CD3DX12_RESOURCE_BARRIER presentResourceBarrier =
			CD3DX12_RESOURCE_BARRIER::Transition(m_deviceResources->GetRenderTarget(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		m_commandList->ResourceBarrier(1, &presentResourceBarrier);
	}
	PIXEndEvent(m_commandList.Get());

	DX::ThrowIfFailed(m_commandList->Close());

	// Execute the command list.
	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_deviceResources->GetCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	return true;
}


void Sample3DSceneRenderer::KeyEvent(Windows::UI::Core::KeyEventArgs^ args)
{
	switch (args->VirtualKey)
	{
		// Control the camera's angle
		case Windows::System::VirtualKey::Up:
		{
			m_cameraPitch += .03f;
			break;
		}
		case Windows::System::VirtualKey::Down:
		{
			m_cameraPitch -= .03f;
			break;
		}
		case Windows::System::VirtualKey::Left:
		{
			m_cameraYaw += 0.03f;
			break;
		}
		case Windows::System::VirtualKey::Right:
		{
			m_cameraYaw -= 0.03f;
			break;
		}

		// Control the camera's position
		case Windows::System::VirtualKey::W:
		{
			eye.f[2] -= .1;
			at.f[2] -= .1;
			break;
		}
		case Windows::System::VirtualKey::S:
		{
			eye.f[2] += .1;
			at.f[2] += .1;
			break;
		}
		case Windows::System::VirtualKey::A:
		{
			eye.f[0] -= .1;
			at.f[0] -= .1;
			break;
		}
		case Windows::System::VirtualKey::D:
		{
			eye.f[0] += .1;
			at.f[0] += .1;
			break;
		}
		case Windows::System::VirtualKey::Q:
		{
			eye.f[1] += .1;
			at.f[1] += .1;
			break;
		}
		case Windows::System::VirtualKey::E:
		{
			eye.f[1] -= .1;
			at.f[1] -= .1;
			break;
		}

		// Controls for manipulating the box
		case Windows::System::VirtualKey::P:
		{
			boxHeight += .1;
			break;
		}
	}
}

// Generate a simple black and white checkerboard texture.
std::vector<UINT8> Sample3DSceneRenderer::GenerateTextureData()
{
	const UINT rowPitch = TextureWidth * TexturePixelSize;
	const UINT cellPitch = rowPitch >> 3;		// The width of a cell in the checkboard texture.
	const UINT cellHeight = TextureWidth >> 3;	// The height of a cell in the checkerboard texture.
	const UINT textureSize = rowPitch * TextureHeight;
	std::vector<UINT8> data(textureSize);

	for (UINT n = 0; n < textureSize; n += TexturePixelSize)
	{
		UINT x = n % rowPitch;
		UINT y = n / rowPitch;
		UINT i = x / cellPitch;
		UINT j = y / cellHeight;

		if (i % 2 == j % 2)
		{
			data[n] = 0x00;		// R
			data[n + 1] = 0x00;	// G
			data[n + 2] = 0x00;	// B
			data[n + 3] = 0xff;	// A
		}
		else
		{
			data[n] = 0xff;		// R
			data[n + 1] = 0xff;	// G
			data[n + 2] = 0xff;	// B
			data[n + 3] = 0xff;	// A
		}
	}

	return data;
}


