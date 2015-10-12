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

//  VertexTextureCoordinate
//  XMFLOAT3, XMFLOAT2

void ModelImporter::importObject()
{
	float scalingFactor = .1;
	string line;
	ifstream myfile("diamond.obj");
	// Read in all of the lines of the obj file
	if (myfile.is_open())
	{
		bool readingVerts = false;
		vector<VertexTextureCoordinate> vertices;

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
					XMFLOAT3(stof(vertexPosStringTokens[1]) * scalingFactor, stof(vertexPosStringTokens[2]) * scalingFactor, stof(vertexPosStringTokens[3]) * scalingFactor),
					XMFLOAT2(0.5f, 0.5f)
				};
				vertices.push_back(thisVert);
			}
		}
		myfile.close();
	}

	else cout << "Unable to open file";
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