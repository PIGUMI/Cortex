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
	/**
	 * @brief ANSI•¶Žš—ñ‚ðUTF-8•¶Žš—ñ‚É•ÏŠ·‚·‚é
	 * @param ansiStr 
	 * @return 
	 */
	std::string AnsiToUtf8(const std::string& ansiStr);
	/**
	 * @brief UTF-8•¶Žš—ñ‚ðANSI•¶Žš—ñ‚É•ÏŠ·‚·‚é
	 * @param utf8Str 
	 * @return 
	 */
	std::string Utf8ToAnsi(const std::string& utf8Str);
}