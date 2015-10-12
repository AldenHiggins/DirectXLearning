#include "ModelImporter.h"
#include "pch.h"
#include "..\Common\DirectXHelper.h"
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
	string line;
	ifstream myfile("diamond.obj");
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
				cout << line << '\n';
				vector<string> vertexPosStringTokens = tokenizeString(line);
				vertexPosStringTokens[0] = vertexPosStringTokens[0];
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