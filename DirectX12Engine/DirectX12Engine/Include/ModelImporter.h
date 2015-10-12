#pragma once

#include <vector>
#include <string>
#include "Content\ShaderStructures.h"

namespace DirectX12Engine
{
	class ModelImporter 
	{
	public:
		std::vector<VertexTextureCoordinate> importObject();

	private:
		std::vector<std::string> tokenizeString(std::string inputString);
	
	};

}
