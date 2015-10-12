#pragma once

#include <vector>
#include <string>
#include "Content\ShaderStructures.h"

namespace DirectX12Engine
{
	struct ImportStructure
	{
		std::vector<VertexTextureCoordinate> vertices;
		std::vector<unsigned short> indices;
	};

	class ModelImporter 
	{
	public:
		ImportStructure importObject();

	private:
		std::vector<std::string> tokenizeString(std::string inputString);
	
	};

}
