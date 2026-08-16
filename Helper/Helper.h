#pragma once
#include <nlohmann/json.hpp>
#include <Windows.h>
#include <string>

namespace Helper
{
	std::string Utf8ToConsole(const std::string& utf8Str);
	size_t Utf8SeqLen(unsigned  char lead);
	size_t InCompleteTailLen(const std::string& buf);
	std::string StripThinkBlock(const std::string& text);
}