#pragma once

#include <vector>
#include <string>
#include "Content\ShaderStructures.h"

namespace DirectX12Engine
{
	struct ObjectDescription
	{
		unsigned short numberIndices;
		unsigned short numberVertices;
	};

	struct ImportStructure
	{
		std::vector<VertexTextureCoordinate> vertices;
		std::vector<unsigned short> indices;
		std::vector<ObjectDescription> objects;
	};

	class ModelImporter 
	{
	public:
		static ImportStructure importObjectObjFile(std::string filename, float scaleFactor);



	private:
		static std::vector<std::string> tokenizeString(std::string inputString);
	
	};

}
