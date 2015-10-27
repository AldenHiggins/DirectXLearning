#include "ModelImporter.h"
#include "pch.h"
#include "..\Common\DirectXHelper.h"
#include "Content\ShaderStructures.h"
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>

using namespace std;
using namespace DirectX12Engine;
using namespace DirectX;

void ModelImporter::importObjectFBXFile()
{
	if (m_pFbxSdkManager == nullptr)
	{
		m_pFbxSdkManager = FbxManager::Create();

		FbxIOSettings* pIOsettings = FbxIOSettings::Create(m_pFbxSdkManager, IOSROOT);
		m_pFbxSdkManager->SetIOSettings(pIOsettings);
	}

	FbxImporter* pImporter = FbxImporter::Create(m_pFbxSdkManager, "");
	FbxScene* pFbxScene = FbxScene::Create(m_pFbxSdkManager, "");

	bool bSuccess = pImporter->Initialize("humanoid.fbx", -1, m_pFbxSdkManager->GetIOSettings());

	if (!bSuccess)
	{
		return;
	}

	bSuccess = pImporter->Import(pFbxScene);
	if (!bSuccess)
	{
		return;
	}

	pImporter->Destroy();

	FbxNode* pFbxRootNode = pFbxScene->GetRootNode();

	if (pFbxRootNode)
	{
		for (int i = 0; i < pFbxRootNode->GetChildCount(); i++)
		{
			FbxNode* pFbxChildNode = pFbxRootNode->GetChild(i);

			if (pFbxChildNode->GetNodeAttribute() == NULL)
			{
				continue;
			}

			FbxNodeAttribute::EType AttributeType = pFbxChildNode->GetNodeAttribute()->GetAttributeType();

			if (AttributeType != FbxNodeAttribute::eMesh)
			{
				continue;
			}

			FbxMesh* pMesh = (FbxMesh*)pFbxChildNode->GetNodeAttribute();

			FbxVector4* pVertices = pMesh->GetControlPoints();

			for (int j = 0; j < pMesh->GetPolygonCount(); j++)
			{
				int iNumVertices = pMesh->GetPolygonSize(j);
				assert(iNumVertices == 3);

				for (int k = 0; k < iNumVertices; k++)
				{
					int iControlPointIndex = pMesh->GetPolygonVertex(j, k);

					XMFLOAT3 thisVertex((float)pVertices[iControlPointIndex].mData[0], (float)pVertices[iControlPointIndex].mData[1], (float)pVertices[iControlPointIndex].mData[2]);

					thisVertex = thisVertex;
				}
			}

		}
	}
}

ImportStructure ModelImporter::importObjectObjFile(string filename, float scaleFactor)
{
	// Structures that will be filled up while reading in the obj file
	vector<VertexTextureCoordinate> vertices;
	vector<unsigned short> indices;

	string line;
	ifstream myfile(filename);

	// Read in all of the lines of the obj file
	if (myfile.is_open())
	{
		bool readingVerts = false;

		while (getline(myfile, line))
		{
			// If the line begins with a # it is a comment and can be ignored
			if (line[0] == '#')
			{
				continue;
			}

			// Check to see if this line contains vertex information
			if (line[0] == 'v')
			{
				// Tokenize the string to extract the three vertex floats
				vector<string> vertexPosStringTokens = tokenizeString(line);
				// Add the vertex to the list of vertices
				VertexTextureCoordinate thisVert = 
				{
					XMFLOAT3(stof(vertexPosStringTokens[1]) * scaleFactor, stof(vertexPosStringTokens[2]) * scaleFactor, stof(vertexPosStringTokens[3]) * scaleFactor),
					XMFLOAT2(0.5f, 0.5f)
				};
				vertices.push_back(thisVert);
			}

			// Check to see if this line contains indices denoting a face
			if (line[0] == 'f')
			{
				// Tokenize the string to extract the indices
				vector<string> indicesStringTokens = tokenizeString(line);

				indices.push_back((unsigned short)stof(indicesStringTokens[1]) - 1);
				indices.push_back((unsigned short)stof(indicesStringTokens[2]) - 1);
				indices.push_back((unsigned short)stof(indicesStringTokens[3]) - 1);
			}
		}
		myfile.close();
	}
	else
	{
		cout << "Unable to open file";
	}

	ObjectDescription thisObject = { (unsigned short) indices.size(), (unsigned short) vertices.size() };
	vector<ObjectDescription> objectDescriptions;
	objectDescriptions.push_back(thisObject);
	ImportStructure returnStruct = { vertices, indices, objectDescriptions };

	return returnStruct;
}

vector<string> ModelImporter::tokenizeString(string inputString)
{
	string buf; // Have a buffer string
	stringstream ss(inputString); // Insert the string into a stream

	vector<string> tokens; // Create vector to hold our words

	while (ss >> buf)
		tokens.push_back(buf);

	return tokens;
}