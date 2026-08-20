#pragma once
/*
* llama.cppを使用したLLMの基底クラス
*/

#include "Helper.h"
#include <string>
#include <nlohmann/json.hpp>
#include <map>

// llama.hの実体はBaseLLM.cppでのみincludeする(ポインタでしか使わないため)
// これによりBaseLLM.hを利用する側のプロジェクトはllama.cppのインクルードパスを設定しなくて済む
struct llama_model;
struct llama_context;
struct llama_vocab;
struct llama_sampler;

using ordered_json = nlohmann::ordered_json;

class BaseLLM
{
public:
	class ModelLoader
	{
	public:
		static ModelLoader* GetInstance();
		static void Release();
	public:
		llama_model* LoadModel(const char* modelPath, int gpuLayer);
		// modelPathの参照カウントを-1し、0になったら実際にGPUメモリから解放する
		void ReleaseModel(const char* modelPath);
		void FreeModel(const char* modelPath);
	private:
		ModelLoader() = default;
		~ModelLoader();
	private:
		std::map<std::string, llama_model*> m_ModelMap;
		std::map<std::string, int> m_RefCounts;
	private:
		static ModelLoader* m_pInstance;
	};
public:
	BaseLLM(const char* modelPath, ordered_json* schema,int gpuLayer = 99,int contextSize = 4096,int BatchSize = 512,int maxTokens = 1024);
	~BaseLLM();
public:
	/**
	 * @brief BaseLLMが有効かどうかを返すエラーが発生している場合はfalseを返す
	 * @return true BaseLLMが有効である
	 */
	bool IsActive() const { return m_bActive; }

	/**
	 * @brief BaseLLMにプロンプトを渡して処理する
	 * @param prompt　処理するプロンプト
	 * @param maxTokens　最大トークン数
	 * @return　処理結果の文字列
	 */
	std::string ProcessPrompt(const std::string& prompt, int maxTokens = 1024);

	/**
	 * @brief contextとsamplerが読み込まれているか(Unload後はfalse)
	 */
	bool IsLoaded() const { return m_pLlmContext != nullptr; }

	/**
	 * @brief GPUメモリ節約のため、contextとsampler、および他に使用者がいなければモデル本体も解放する
	 */
	void Unload();

	/**
	 * @brief Unloadで解放した状態から、モデル・context・samplerを再度読み込む
	 * @return 成功したらtrue
	 */
	bool Reload();

protected:
	/**
	 * @brief システムプロンプトを設定する
	 * @param systemPrompt システムプロンプトの文字列
	 */
	void SetSystemPrompt(const std::string& systemPrompt) { m_sSystemPrompt = systemPrompt; }

private:

	/**
	 * @brief コンテキストを作成する
	 * @param contextSize
	 * @param BatchSize 
	 * @return 
	 */
	bool CreateContext(int contextSize, int BatchSize);

	/**
	 * @brief サンプラーを作成する
	 * @param schema 
	 * @return 
	 */
	bool CreateSampler(ordered_json* schema);
protected:
	bool m_bActive = false;
	std::string m_sSystemPrompt = "Please respond in Japanese.";
	std::string m_sModelPath;
private:
	// Reload()でcontext/samplerを作り直すために構築時の設定を保持しておく
	ordered_json* m_pSchema = nullptr;
	int m_GpuLayer = 99;
	int m_ContextSize = 4096;
	int m_BatchSize = 512;
protected:
	llama_model* m_pLlmModel = nullptr;
	llama_context* m_pLlmContext = nullptr;
	const llama_vocab* m_pLlmVocab = nullptr;
	llama_sampler* m_pLlmSampler = nullptr;
	const char* m_pLlmTmpl = nullptr;
};