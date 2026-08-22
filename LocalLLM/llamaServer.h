#pragma once

#include <string>
#include <functional>


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

	/**
	 * @brief llama-serverのOpenAI互換エンドポイントにプロンプトを投げて応答を受け取る(ストリーミング)
	 * @param utf8UserMessage 
	 * @param onDelta 
	 * @return 
	 */
	std::string CallLlamaServerStream(const std::string& utf8UserMessage,std::function<void(const std::string& deltaUtf8)> onDelta);
}