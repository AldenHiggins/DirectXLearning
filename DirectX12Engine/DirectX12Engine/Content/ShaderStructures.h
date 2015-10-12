#pragma once

#include "pch.h"

namespace DirectX12Engine
{
	// Constant buffer used to send MVP matrices to the vertex shader.
	struct ViewProjectionConstantBuffer
	{
		DirectX::XMFLOAT4X4 view;
		DirectX::XMFLOAT4X4 projection;
	};

	// Constant buffer used to send the model matrices of objects to the vertex shader
	struct ModelMatrixConstantBuffer
	{
		DirectX::XMFLOAT4X4 model;
	};

	// Used to send per-vertex data to the vertex shader.
	struct VertexPositionColor
	{
		DirectX::XMFLOAT3 pos;
		DirectX::XMFLOAT3 color;
	};

	// Send vertex data along with texture coordinates to the vertex shader
	struct VertexTextureCoordinate
	{
		DirectX::XMFLOAT3 pos;
		DirectX::XMFLOAT2 uv;
	};
}