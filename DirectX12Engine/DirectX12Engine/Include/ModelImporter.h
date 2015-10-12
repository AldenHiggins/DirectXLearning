#pragma once

#include <vector>
#include <string>

namespace DirectX12Engine
{
	class ModelImporter 
	{
	public:
		void importObject();

	private:
		std::vector<std::string> tokenizeString(std::string inputString);
	
	};

}
