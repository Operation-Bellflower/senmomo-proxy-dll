#include "pch.h"

std::string wide_string_to_narrow(std::wstring input)
{
	const int chars = WideCharToMultiByte(932, 0, input.c_str(), -1, nullptr, 0, nullptr, nullptr);
	std::string output;
	output.resize(chars);
	WideCharToMultiByte(932, 0, input.c_str(), -1, output.data(), chars, nullptr, nullptr);
	return output;
};

std::wstring narrow_string_to_wide(std::string input)
{
	const int chars = MultiByteToWideChar(932, 0, input.c_str(), -1, nullptr, 0);
	std::wstring output;
	output.resize(chars);
	MultiByteToWideChar(932, 0, input.c_str(), -1, output.data(), chars);
	return output;
};

std::wstring utf8_to_wide(std::string input)
{
	const int chars = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, nullptr, 0);
	std::wstring output;
	output.resize(chars);
	MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, output.data(), chars);
	return output;
};
