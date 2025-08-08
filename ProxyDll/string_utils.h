#pragma once
#include "pch.h"

std::string wide_string_to_narrow(std::wstring input);
std::wstring narrow_string_to_wide(std::string input);
std::wstring utf8_to_wide(std::string input);
