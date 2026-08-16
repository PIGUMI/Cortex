#include "BaseLLM.h"
#include "json-schema-to-grammar.h"
#include <iostream>

BaseLLM::ModelLoader* BaseLLM::ModelLoader::m_pInstance = nullptr;

static void MyGgmlLogCallback(ggml_log_level level, const char* text, void* /*user_data*/)
{
	if (level >= GGML_LOG_LEVEL_WARN)
	{
		fputs(text, stderr);
	}
}

BaseLLM::BaseLLM(const char* modelPath, ordered_json* schema, int gpuLayer, int contextSize, int BatchSize, int maxTokens)
	: m_sModelPath(modelPath), m_pSchema(schema), m_GpuLayer(gpuLayer), m_ContextSize(contextSize), m_BatchSize(BatchSize)
{
	ggml_log_set(MyGgmlLogCallback, nullptr);

	m_pLlmModel = ModelLoader::GetInstance()->LoadModel(modelPath, gpuLayer);
	if (!m_pLlmModel)
	{
		m_bActive = false;
		std::cout << "LLMモデルのロードに失敗しました。" << std::endl;
		return;
	}
	if (!CreateContext(contextSize, BatchSize))
	{
		m_bActive = false;
		std::cout << "LLMコンテキストの作成に失敗しました。" << std::endl;
		return;
	}
	if(!CreateSampler(schema))
	{
		m_bActive = false;
		std::cout << "LLMサンプラーの作成に失敗しました。" << std::endl;
		return;
	}

}

BaseLLM::~BaseLLM()
{
	if(m_pLlmSampler)
	{
		llama_sampler_free(m_pLlmSampler);
		m_pLlmSampler = nullptr;
	}
	if(m_pLlmVocab)
	{
		m_pLlmVocab = nullptr;
	}
	if(m_pLlmContext)
	{
		llama_free(m_pLlmContext);
		m_pLlmContext = nullptr;
	}
	if(m_pLlmSampler)
	{
		llama_sampler_free(m_pLlmSampler);
		m_pLlmSampler = nullptr;
	}
}

void BaseLLM::Unload()
{
	if (!IsLoaded())
	{
		return;
	}

	if (m_pLlmSampler)
	{
		llama_sampler_free(m_pLlmSampler);
		m_pLlmSampler = nullptr;
	}
	if (m_pLlmContext)
	{
		llama_free(m_pLlmContext);
		m_pLlmContext = nullptr;
	}
	m_pLlmVocab = nullptr;
	m_pLlmTmpl = nullptr;

	if (m_pLlmModel)
	{
		ModelLoader::GetInstance()->ReleaseModel(m_sModelPath.c_str());
		m_pLlmModel = nullptr;
	}
}

bool BaseLLM::Reload()
{
	if (IsLoaded())
	{
		return true;
	}

	m_pLlmModel = ModelLoader::GetInstance()->LoadModel(m_sModelPath.c_str(), m_GpuLayer);
	if (!m_pLlmModel)
	{
		return false;
	}
	if (!CreateContext(m_ContextSize, m_BatchSize))
	{
		return false;
	}
	if (!CreateSampler(m_pSchema))
	{
		return false;
	}
	return true;
}

std::string BaseLLM::ProcessPrompt(const std::string& prompt, int maxTokens)
{
	llama_memory_clear(llama_get_memory(m_pLlmContext), true);
	llama_sampler_reset(m_pLlmSampler);

	llama_chat_message messages[] =
	{
		{"system", m_sSystemPrompt.c_str()},
		{"user", prompt.c_str()}
	};

	std::vector<char> buf(1024);
	int32_t n = llama_chat_apply_template(m_pLlmTmpl, messages, 2, true, buf.data(), (int32_t)buf.size());

	if (n > (int32_t)buf.size())
	{
		buf.resize(n);
		n = llama_chat_apply_template(m_pLlmTmpl, messages, 2, true, buf.data(), (int32_t)buf.size());
	}

	std::string formattede_prompt(buf.data(), n);

	int n_tokens = -llama_tokenize(m_pLlmVocab, formattede_prompt.c_str(), (int32_t)formattede_prompt.size(), nullptr, 0, true, true);

	int n_ctx = (int)llama_n_ctx(m_pLlmContext);
	// システムプロンプト自体が肥大化し続けており、n_ctxの実際の使用状況が見えないまま
	// "prompt too long"に頻繁に引っかかる事故が繰り返し起きているため、毎回のトークン数を
	// 記録しておく(システムプロンプトの成長ペースと実際の残り予算を把握するための恒久的な診断ログ)。
	std::cout << "[tokens] n_tokens=" << n_tokens << " / n_ctx=" << n_ctx << " (maxTokens=" << maxTokens << ")" << std::endl;
	if (n_tokens <= 0 || n_tokens + maxTokens >= n_ctx)
	{
		return "Error: prompt too long for the model context window (" + std::to_string(n_tokens) +
			" tokens, context limit " + std::to_string(n_ctx) + "). Shorten the conversation or investigation history and try again.";
	}

	std::vector<llama_token> tokens(n_tokens);

	llama_tokenize(m_pLlmVocab, formattede_prompt.c_str(), (int32_t)formattede_prompt.size(), tokens.data(), n_tokens, true, true);

	llama_batch batch = llama_batch_get_one(tokens.data(), n_tokens);
	llama_decode(m_pLlmContext, batch);

	int generated_tokens = 0;
	std::string PendingUtf8, full_Response;
	PendingUtf8.clear();
	full_Response.clear();

	while (generated_tokens < maxTokens)
	{
		llama_token new_token = llama_sampler_sample(m_pLlmSampler, m_pLlmContext, -1);
		if (llama_vocab_is_eog(m_pLlmVocab, new_token)) break;

		char buffer[256];
		int n = llama_token_to_piece(m_pLlmVocab, new_token, buffer, sizeof(buffer), 0, true);
		if (n < 0)
		{
			std::vector<char> largeBuffer(static_cast<size_t>(-n));
			n = llama_token_to_piece(m_pLlmVocab, new_token, largeBuffer.data(), (int32_t)largeBuffer.size(), 0, true);
			if (n > 0)
			{
				PendingUtf8.append(largeBuffer.data(), n);
			}
		}
		else if (n > 0)
		{
			PendingUtf8.append(buffer, n);
		}

		size_t incomplete = Helper::InCompleteTailLen(PendingUtf8);
		size_t ready = PendingUtf8.size() - incomplete;
		if (ready > 0)
		{
			full_Response.append(PendingUtf8.data(), ready);
			PendingUtf8.erase(0, ready);
		}

		llama_batch next = llama_batch_get_one(&new_token, 1);
		if (llama_decode(m_pLlmContext, next) != 0)break;
		++generated_tokens;
	}
	if (!PendingUtf8.empty())
	{
		full_Response.append(PendingUtf8);
		PendingUtf8.clear();
	}

	return Helper::StripThinkBlock(full_Response);
}


bool BaseLLM::CreateContext(int contextSize, int BatchSize)
{
	try
	{
		llama_context_params context_params = llama_context_default_params();
		context_params.n_ctx = contextSize;
		context_params.n_batch = BatchSize;

		m_pLlmContext = llama_init_from_model(m_pLlmModel, context_params);
		if(m_pLlmContext)
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	catch (const std::exception&)
	{
		return false;
	}
	catch (...)
	{
		return false;
	}
}

bool BaseLLM::CreateSampler(ordered_json* schema)
{
	try
	{
		m_pLlmVocab = llama_model_get_vocab(m_pLlmModel);

		llama_sampler_chain_params sparams = llama_sampler_chain_default_params();
		m_pLlmSampler = llama_sampler_chain_init(sparams);
		if (schema)
		{
			ordered_json schema_copy = *schema;
			std::string grammar_str = json_schema_to_grammar(schema_copy);
			llama_sampler* grammar_sampler = llama_sampler_init_grammar(m_pLlmVocab, grammar_str.c_str(), "root");
			if (!grammar_sampler)return false;
			llama_sampler_chain_add(m_pLlmSampler, grammar_sampler);
		}
		llama_sampler_chain_add(m_pLlmSampler, llama_sampler_init_penalties(llama_vocab_n_tokens(m_pLlmVocab), 64, 1.1f, 0.0f, 0.0f));
		llama_sampler_chain_add(m_pLlmSampler, llama_sampler_init_greedy());

		if(!m_pLlmSampler)return false;

		m_pLlmTmpl = llama_model_chat_template(m_pLlmModel, nullptr);
		if (!m_pLlmTmpl)return false;
		return true;
	}
	catch (const std::exception&)
	{
		return false;
	}
	catch (...)
	{
		return false;
	}
}


BaseLLM::ModelLoader* BaseLLM::ModelLoader::GetInstance()
{
	if (!m_pInstance)
	{
		m_pInstance = new ModelLoader();
	}
	return m_pInstance;
}


void BaseLLM::ModelLoader::Release()
{
	if (m_pInstance)
	{
		delete m_pInstance;
		m_pInstance = nullptr;
	}
}

llama_model* BaseLLM::ModelLoader::LoadModel(const char* modelPath, int gpuLayer)
{
	auto it = m_ModelMap.find(modelPath);
	if (it != m_ModelMap.end())
	{
		++m_RefCounts[modelPath];
		return it->second;
	}
	else
	{
		llama_model_params params = llama_model_default_params();
		params.n_gpu_layers = gpuLayer;
		llama_model* model = llama_model_load_from_file(modelPath, params);
		if (model)
		{
			m_ModelMap[modelPath] = model;
			m_RefCounts[modelPath] = 1;
			return model;
		}
		else
		{
			return nullptr;
		}
	}
}

void BaseLLM::ModelLoader::ReleaseModel(const char* modelPath)
{
	auto refIt = m_RefCounts.find(modelPath);
	if (refIt == m_RefCounts.end())
	{
		return;
	}

	--refIt->second;
	if (refIt->second <= 0)
	{
		m_RefCounts.erase(refIt);
		FreeModel(modelPath);
	}
}

void BaseLLM::ModelLoader::FreeModel(const char* modelPath)
{
	auto it = m_ModelMap.find(modelPath);
	if (it != m_ModelMap.end())
	{
		llama_model_free(it->second);
		m_ModelMap.erase(it);
	}
}

BaseLLM::ModelLoader::~ModelLoader()
{
	for (auto& pair : m_ModelMap)
	{
		llama_model_free(pair.second);
	}
	m_ModelMap.clear();
}
