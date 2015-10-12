#include "pch.h"
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

// Loads vertex and pixel shaders from files and instantiates the cube geometry.
Sample3DSceneRenderer::Sample3DSceneRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
	m_loadingComplete(false),
	m_radiansPerSecond(XM_PIDIV4),	// rotate 45 degrees per second
	m_tracking(false),
	m_mappedConstantBuffer(nullptr),
	m_deviceResources(deviceResources)
{
	ZeroMemory(&m_constantBufferData, sizeof(m_constantBufferData));

	m_camera.Init({ 0, 2, 5 });
	m_camera.SetMoveSpeed(5.0f);

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
		CD3DX12_DESCRIPTOR_RANGE range[3];
		CD3DX12_ROOT_PARAMETER parameter[3];
		
		range[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
		range[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
		parameter[0].InitAsDescriptorTable(1, &range[0], D3D12_SHADER_VISIBILITY_VERTEX);
		parameter[1].InitAsDescriptorTable(1, &range[1], D3D12_SHADER_VISIBILITY_PIXEL);
		parameter[2].InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_VERTEX);

		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =	D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT; 

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
		state.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
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

		// Call the model importer
		m_objectData = m_modelImporter.importObject("teapot.obj", 0.03f);
		std::vector<VertexTextureCoordinate> vertices = m_objectData.vertices;

		// Add on the cube vertices
		vertices.push_back({ XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT2(0.0f, 0.0f) });
		vertices.push_back({ XMFLOAT3(-0.5f, -0.5f,  0.5f), XMFLOAT2(1.0f, 0.0f) });
		vertices.push_back({ XMFLOAT3(-0.5f,  0.5f, -0.5f), XMFLOAT2(0.0f, 1.0f) });
		vertices.push_back({ XMFLOAT3(-0.5f,  0.5f,  0.5f), XMFLOAT2(1.0f, 1.0f) });
		vertices.push_back({ XMFLOAT3(0.5f, -0.5f, -0.5f), XMFLOAT2(1.0f, 0.0f) });
		vertices.push_back({ XMFLOAT3(0.5f, -0.5f,  0.5f), XMFLOAT2(0.0f, 0.0f) });
		vertices.push_back({ XMFLOAT3(0.5f,  0.5f, -0.5f), XMFLOAT2(1.0f, 1.0f) });
		vertices.push_back({ XMFLOAT3(0.5f,  0.5f,  0.5f), XMFLOAT2(0.0f, 1.0f) });

		// Floor vertices
		vertices.push_back({ XMFLOAT3(-3.0f, 0.0f, -3.0f), XMFLOAT2(0.0f, 0.0f) });
		vertices.push_back({ XMFLOAT3(-3.0f, 0.0f,  3.0f), XMFLOAT2(0.0f, 1.0f) });
		vertices.push_back({ XMFLOAT3(3.0f,  0.0f, 3.0f), XMFLOAT2(1.0f, 1.0f) });
		vertices.push_back({ XMFLOAT3(3.0f,  0.0f,  -3.0f), XMFLOAT2(1.0f, 0.0f) });

		VertexTextureCoordinate *vertexArray = &vertices[0];

		const UINT vertexBufferSize = sizeof(VertexTextureCoordinate) * vertices.size();

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
			vertexData.pData = reinterpret_cast<BYTE*>(vertexArray);
			vertexData.RowPitch = vertexBufferSize;
			vertexData.SlicePitch = vertexData.RowPitch;

			UpdateSubresources(m_commandList.Get(), m_vertexBuffer.Get(), vertexBufferUpload.Get(), 0, 0, 1, &vertexData);

			CD3DX12_RESOURCE_BARRIER vertexBufferResourceBarrier =
				CD3DX12_RESOURCE_BARRIER::Transition(m_vertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
			m_commandList->ResourceBarrier(1, &vertexBufferResourceBarrier);
		}

		// Start the index buffer with data taken in from the importer
		std::vector<unsigned short> indices = m_objectData.indices;

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

		for (int index = 0; index < 42; index++)
		{
			indices.push_back(cubeIndices[index]);
		}

		unsigned short *indicesPointer = &indices[0];

		const UINT indexBufferSize = sizeof(unsigned short) * indices.size();

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
			indexData.pData = reinterpret_cast<BYTE*>(indicesPointer);
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
			heapDesc.NumDescriptors = DX::c_frameCount
				+ 1 // +1 for the checkerboard texture
				+ 2; // + 2 for the two model matrices
			heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			// This flag indicates that this descriptor heap can be bound to the pipeline and that descriptors contained in it can be referenced by a root table.
			heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			DX::ThrowIfFailed(d3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_cbvHeap)));

			m_cbvHeap->SetName(L"Constant Buffer View Descriptor Heap");
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

		// Generate the two constant buffer views that contain the model matrices for each of the objects
		{
			CD3DX12_RESOURCE_DESC modelBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(c_alignedModelConstantBufferSize);
			
			DX::ThrowIfFailed(d3dDevice->CreateCommittedResource(
				&uploadHeapProperties,
				D3D12_HEAP_FLAG_NONE,
				&constantBufferDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&m_constantBuffer)));

			D3D12_GPU_VIRTUAL_ADDRESS modelBufferGpuAddress = m_constantBuffer->GetGPUVirtualAddress();
			CD3DX12_CPU_DESCRIPTOR_HANDLE modelBufferCpuAddress(m_cbvHeap->GetCPUDescriptorHandleForHeapStart());

			modelBufferCpuAddress.Offset(3 * m_cbvDescriptorSize);
			modelBufferGpuAddress += (3 * c_alignedConstantBufferSize);

			D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
			desc.BufferLocation = modelBufferGpuAddress;
			desc.SizeInBytes = c_alignedModelConstantBufferSize;
			d3dDevice->CreateConstantBufferView(&desc, modelBufferCpuAddress);

			modelBufferCpuAddress.Offset(m_cbvDescriptorSize);
			modelBufferGpuAddress += (c_alignedModelConstantBufferSize);

			D3D12_CONSTANT_BUFFER_VIEW_DESC otherBufferDesc;
			otherBufferDesc.BufferLocation = modelBufferGpuAddress;
			otherBufferDesc.SizeInBytes = c_alignedModelConstantBufferSize;
			d3dDevice->CreateConstantBufferView(&otherBufferDesc, modelBufferCpuAddress);
		}


		// Create an upload heap to load the texture onto the GPU. ComPtr's are CPU objects
		// but this heap needs to stay in scope until the GPU work is complete. We will
		// synchronize with the GPU at the end of this method before the ComPtr is destroyed.
		ComPtr<ID3D12Resource> textureUploadHeap;
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

			// Describe and create a SRV for the texture.
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Format = textureDesc.Format;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = 1;
			CD3DX12_CPU_DESCRIPTOR_HANDLE cbvSrvHandle(m_cbvHeap->GetCPUDescriptorHandleForHeapStart(), 5, m_cbvDescriptorSize);
			d3dDevice->CreateShaderResourceView(m_texture.Get(), &srvDesc, cbvSrvHandle);
		}

		// Close the command list and execute it to begin the vertex/index buffer copy into the GPU's default heap.
		DX::ThrowIfFailed(m_commandList->Close());
		ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
		m_deviceResources->GetCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

		// Create vertex/index buffer views.
		m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
		m_vertexBufferView.StrideInBytes = sizeof(VertexTextureCoordinate);
		m_vertexBufferView.SizeInBytes = sizeof(VertexTextureCoordinate) * vertices.size();

		m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
		m_indexBufferView.SizeInBytes = sizeof(unsigned short) * indices.size();
		m_indexBufferView.Format = DXGI_FORMAT_R16_UINT;

		// Wait for the command list to finish executing; the vertex/index buffers need to be uploaded to the GPU before the upload resources go out of scope.
		m_deviceResources->WaitForGpu();
	});

	createAssetsTask.then([this]() {
		m_loadingComplete = true;
	});
}

// Renders one frame using the vertex and pixel shaders.F
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
		// Bind the shader resource view to the correct slot in the descriptor table
		CD3DX12_GPU_DESCRIPTOR_HANDLE modelCbvGpuHandle(m_cbvHeap->GetGPUDescriptorHandleForHeapStart(), 5, m_cbvDescriptorSize);
		m_commandList->SetGraphicsRootDescriptorTable(1, modelCbvGpuHandle);
		// Bind the first model matrix to the descriptor table
		m_commandList->SetGraphicsRootConstantBufferView(2, m_constantBuffer->GetGPUVirtualAddress() + (3 * c_alignedConstantBufferSize));

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

		// Update the ground's model matrix
		ModelMatrixConstantBuffer *objectModelMatrix = (ModelMatrixConstantBuffer *)(m_mappedConstantBuffer + (3 * c_alignedConstantBufferSize));
		XMStoreFloat4x4(&objectModelMatrix->model, XMMatrixIdentity());
		// Draw the ground
		m_commandList->DrawIndexedInstanced(6, 1, 36 + m_objectData.objects[0].numberIndices, m_objectData.objects[0].numberVertices, 0);
		// Draw the diamond
		m_commandList->DrawIndexedInstanced(m_objectData.objects[0].numberIndices, 1, 0, 0, 0);

		// Switch the model matrix to point to the cube's matrix slot in the descriptor heap
		m_commandList->SetGraphicsRootConstantBufferView(2, m_constantBuffer->GetGPUVirtualAddress() + (3 * c_alignedConstantBufferSize) + c_alignedModelConstantBufferSize);

		// Update the cube's model matrix
		objectModelMatrix = (ModelMatrixConstantBuffer *)(m_mappedConstantBuffer + (3 * c_alignedConstantBufferSize) + c_alignedModelConstantBufferSize);
		XMStoreFloat4x4(&objectModelMatrix->model, XMMatrixTranspose(XMMatrixTranslation(0.0f, 1.0f, 0.0f)));
		// Draw the cube
		m_commandList->DrawIndexedInstanced(36, 1, m_objectData.objects[0].numberIndices, m_objectData.objects[0].numberVertices, 0);

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

// Initializes view parameters when the window size changes.
void Sample3DSceneRenderer::CreateWindowSizeDependentResources()
{
	Size outputSize = m_deviceResources->GetOutputSize();
	m_aspectRatio = outputSize.Width / outputSize.Height;
	float fovAngleY = 70.0f * XM_PI / 180.0f;

	D3D12_VIEWPORT viewport = m_deviceResources->GetScreenViewport();
	m_scissorRect = { 0, 0, static_cast<LONG>(viewport.Width), static_cast<LONG>(viewport.Height)};

	// This is a simple example of change that can be made when the app is in
	// portrait or snapped view.
	if (m_aspectRatio < 1.0f)
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
		m_aspectRatio,
		0.01f,
		100.0f
		);

	XMFLOAT4X4 orientation = m_deviceResources->GetOrientationTransform3D();
	XMMATRIX orientationMatrix = XMLoadFloat4x4(&orientation);

	XMStoreFloat4x4(
		&m_constantBufferData.projection,
		XMMatrixTranspose(perspectiveMatrix * orientationMatrix)
		);
}

// Called once per frame, rotates the cube and calculates the model and view matrices.
void Sample3DSceneRenderer::Update(DX::StepTimer const& timer)
{
	if (!m_loadingComplete)
	{
		return;
	}

	// Update the camera and the view and projection matrices
	m_camera.Update(static_cast<float>(timer.GetElapsedSeconds()));
	XMStoreFloat4x4(&m_constantBufferData.view, XMMatrixTranspose(m_camera.GetViewMatrix()));
	XMStoreFloat4x4(&m_constantBufferData.projection, XMMatrixTranspose(m_camera.GetProjectionMatrix(0.8f, m_aspectRatio)));

	// Update the constant buffer resource.
	UINT8* destination = m_mappedConstantBuffer + (m_deviceResources->GetCurrentFrameIndex() * c_alignedConstantBufferSize);
	memcpy(destination, &m_constantBufferData, sizeof(m_constantBufferData));
}

void Sample3DSceneRenderer::KeyUpEvent(Windows::UI::Core::KeyEventArgs^ args)
{
	m_camera.OnKeyUp(args);
}

void Sample3DSceneRenderer::KeyEvent(Windows::UI::Core::KeyEventArgs^ args)
{
	m_camera.OnKeyDown(args);
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
