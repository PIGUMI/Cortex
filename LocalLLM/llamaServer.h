#pragma once

#include <string>


/*
* llama-serverのOpenAI互換エンドポイントにプロンプトを投げて応答を受け取る
*/

namespace LocalLLM
{
	/**
	 * @brief llama-serverのOpenAI互換エンドポイントにプロンプトを投げて応答を受け取る
	 * @param utf8UserMessage 
	 * @return 
	 */
	std::string CallLlamaServer(const std::string& utf8UserMessage);
}