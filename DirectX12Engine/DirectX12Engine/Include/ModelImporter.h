#pragma once

#include <vector>
#include <string>
#include "Content\ShaderStructures.h"
#include <fbxsdk.h>

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
		ModelImporter()
		{
			m_pFbxSdkManager = nullptr;
		}

		// Import a model from an obj file
		static ImportStructure importObjectObjFile(std::string filename, float scaleFactor);
		// Import a model from an fbx file
		void importObjectFBXFile();
		
	private:
		// Manages FBX model importation
		FbxManager* m_pFbxSdkManager;

		static std::vector<std::string> tokenizeString(std::string inputString);
	};

}
