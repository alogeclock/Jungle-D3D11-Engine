#pragma once

#include <iostream>
#include <string>

namespace Logger
{
	inline void Log(const std::string& message)
	{
		std::cout << "[LOG] " << message << std::endl;
	}

	inline void Warn(const std::string& message)
	{
		std::cout << "[WARN] " << message << std::endl;
	}

	inline void Error(const std::string& message)
	{
		std::cout << "[ERROR] " << message << std::endl;
	}
}
