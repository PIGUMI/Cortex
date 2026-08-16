#include "BaseLLM.h"

#include <iostream>

// LocalLLMプロジェクト用の最小サンプル
// 使い方: LocalLLM.exe <モデル(.gguf)へのパス> ["プロンプト"]
int main(int argc, char** argv)
{
	if (argc < 2)
	{
		std::cerr << "usage: " << argv[0] << " <model.gguf> [\"prompt\"]" << std::endl;
		return 1;
	}

	const char* modelPath = argv[1];
	const std::string prompt = (argc >= 3) ? argv[2] : "こんにちは";

	BaseLLM llm(modelPath, nullptr);
	if (!llm.IsActive())
	{
		std::cerr << "LLMの初期化に失敗しました" << std::endl;
		return 1;
	}

	std::cout << llm.ProcessPrompt(prompt) << std::endl;

	return 0;
}
