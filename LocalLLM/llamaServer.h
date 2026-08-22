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

	// ストリーミング1回分の実測値(クライアント側で計測)
	struct StreamStats
	{
		size_t tokenCount = 0;        // 受信したトークン(SSEチャンク)数
		double elapsedSeconds = 0.0;  // 最初のトークン受信から最後のトークン受信までの経過時間
		double tokensPerSecond = 0.0; // tokenCount / elapsedSeconds
	};

	/**
	 * @brief llama-serverのOpenAI互換エンドポイントにプロンプトを投げて応答を受け取る(ストリーミング)
	 * @param utf8UserMessage
	 * @param onDelta
	 * @param outStats 非nullの場合、生成速度の実測値を書き込む
	 * @return
	 */
	std::string CallLlamaServerStream(const std::string& utf8UserMessage,
		std::function<void(const std::string& deltaUtf8)> onDelta,
		StreamStats* outStats = nullptr);
}