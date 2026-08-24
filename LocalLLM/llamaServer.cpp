#include "llamaServer.h"
#include "Helper.h"

#include <Windows.h>
#include <winhttp.h>
#include <nlohmann/json.hpp>
#include <vector>
#include <chrono>
#pragma comment(lib, "winhttp.lib")

#define LLAMA_SERVER_HOST L"192.168.0.13"
#define LLAMA_SERVER_PORT (8080)
#define LLAMA_SERVER_API_PATH L"/v1/chat/completions"
#define LLAMA_SERVER_API_KEY "aac7cad858b5df7eea54979d29f76ff96b3d37d4b1f9c7f7"

std::string LocalLLM::CallLlamaServer(const std::string& utf8UserMessage)
{
	nlohmann::json requestBody =
	{
			{"model", "Qwen3.5-9B-UD-Q6_K_XL"},
			{"chat_template_kwargs", {{"enable_thinking", false}}}, // 簡易チャット用に思考過程(reasoning)を無効化
			{"messages", nlohmann::json::array({
				// \uエスケープ(素のASCII)で書くことで、ソースファイルの文字コードに依存せず
				// 確実にUTF-8バイト列を得る(u8リテラルはUnicodeコードポイント基準で常にUTF-8化される)
				{{"role", "system"}, {"content", reinterpret_cast<const char*>(
					u8"日本語で応答してください。") }},
				{{"role", "user"}, {"content", utf8UserMessage}}
			})}
	};
	std::string body = requestBody.dump();

	std::string result;

	HINTERNET hSession = WinHttpOpen(L"Cortex/1.0",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
	if (!hSession)return "ERROR: WinHttpOpen failed";

	WinHttpSetTimeouts(hSession, 0, 60000, 30000, 120000);

	HINTERNET hConnect = WinHttpConnect(hSession, LLAMA_SERVER_HOST, LLAMA_SERVER_PORT, 0);
	if (!hConnect)
	{
		WinHttpCloseHandle(hSession);
		return "ERROR: WinHttpConnect failed";
	}

	HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", LLAMA_SERVER_API_PATH, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
	if (!hRequest)
	{
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return "ERROR: WinHttpOpenRequest failed";
	}

	// 接続を使い回さず、毎回新規接続にする(サーバーがConnection: closeを返すため)
	DWORD disableKeepAlive = WINHTTP_DISABLE_KEEP_ALIVE;
	WinHttpSetOption(hRequest, WINHTTP_OPTION_DISABLE_FEATURE, &disableKeepAlive, sizeof(disableKeepAlive));

	std::wstring headers = L"Content-Type: application/json\r\nAuthorization: Bearer ";

	headers += std::wstring(LLAMA_SERVER_API_KEY, LLAMA_SERVER_API_KEY + strlen(LLAMA_SERVER_API_KEY));

	BOOL sent = WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)headers.size(), (LPVOID)body.c_str(), (DWORD)body.size(), (DWORD)body.size(), 0);
	DWORD sendError = sent ? 0 : GetLastError();

	BOOL received = sent && WinHttpReceiveResponse(hRequest, nullptr);
	DWORD receiveError = (sent && !received) ? GetLastError() : 0;

	if (received)
	{
		std::string responseBody;
		DWORD bytesAvailable = 0;
		do
		{
			if (!WinHttpQueryDataAvailable(hRequest, &bytesAvailable) || bytesAvailable == 0)break;
			std::vector<char> buf(bytesAvailable);
			DWORD bytesRead = 0;
			if (WinHttpReadData(hRequest, buf.data(), bytesAvailable, &bytesRead))responseBody.append(buf.data(), bytesRead);
		} while (bytesAvailable > 0);

		try
		{
			nlohmann::json responseJson = nlohmann::json::parse(responseBody);
			result = responseJson["choices"][0]["message"]["content"].get<std::string>();

		}
		catch (const std::exception& e)
		{
			result = std::string("Error: JSON parse failed - ") + e.what() + " / body=" + responseBody;
		}
	}
	else if (!sent)
	{
		result = "Error: WinHttpSendRequest failed, GetLastError=" + std::to_string(sendError);
	}
	else
	{
		result = "Error: WinHttpReceiveResponse failed, GetLastError=" + std::to_string(receiveError);
	}

	WinHttpCloseHandle(hRequest);
	WinHttpCloseHandle(hConnect);
	WinHttpCloseHandle(hSession);
	return result;

}

std::string LocalLLM::CallLlamaServerStream(const std::string& utf8UserMessage,std::function<void(const std::string& deltaUtf8)> onDelta,StreamStats* outStats)
{
	nlohmann::json requestBody =
	{
			{"model", "Qwen3.5-9B-UD-Q6_K_XL"},
			{"stream", true},
			{"chat_template_kwargs", {{"enable_thinking", false}}}, // 簡易チャット用に思考過程(reasoning)を無効化
			{"messages", nlohmann::json::array({
				{{"role", "system"}, {"content", reinterpret_cast<const char*>(
					u8"日本語で応答してください。") }},
				{{"role", "user"}, {"content", utf8UserMessage}}
			})}
	};
	std::string body = requestBody.dump();

	std::string fullResponse;

	HINTERNET hSession = WinHttpOpen(L"Cortex/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (!hSession)return "ERROR: WinHttpOpen failed";

	WinHttpSetTimeouts(hSession, 0, 60000, 30000, 120000);

	HINTERNET hConnect = WinHttpConnect(hSession, LLAMA_SERVER_HOST, LLAMA_SERVER_PORT, 0);
	if (!hConnect)
	{
		WinHttpCloseHandle(hSession);
		return "ERROR: WinHttpConnect failed";
	}

	HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", LLAMA_SERVER_API_PATH, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
	if (!hRequest)
	{
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return "ERROR: WinHttpOpenRequest failed";
	}

	// 接続を使い回さず、毎回新規接続にする(サーバーがConnection: closeを返すため)
	DWORD disableKeepAlive = WINHTTP_DISABLE_KEEP_ALIVE;
	WinHttpSetOption(hRequest, WINHTTP_OPTION_DISABLE_FEATURE, &disableKeepAlive, sizeof(disableKeepAlive));

	std::wstring headers = L"Content-Type: application/json\r\nAuthorization: Bearer ";
	headers += std::wstring(LLAMA_SERVER_API_KEY, LLAMA_SERVER_API_KEY + strlen(LLAMA_SERVER_API_KEY));

	BOOL sent = WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)headers.size(), (LPVOID)body.c_str(), (DWORD)body.size(), (DWORD)body.size(), 0);
	DWORD sendError = sent ? 0 : GetLastError();

	BOOL received = sent && WinHttpReceiveResponse(hRequest, nullptr);
	DWORD receiveError = (sent && !received) ? GetLastError() : 0;

	if (received)
	{
		std::string sseBuffer;   // 受信したがまだ行として処理していない生バイト列
		std::string pendingUtf8; // 完全なUTF-8文字になるまで保留するバイト列(マルチバイト文字の分割対策)
		bool done = false;

		size_t tokenCount = 0;
		std::chrono::steady_clock::time_point firstTokenTime{};
		std::chrono::steady_clock::time_point lastTokenTime{};
		bool hasFirstToken = false;

		DWORD bytesAvailable = 0;
		do
		{
			if (!WinHttpQueryDataAvailable(hRequest, &bytesAvailable) || bytesAvailable == 0)break;
			std::vector<char> buf(bytesAvailable);
			DWORD bytesRead = 0;
			if (!WinHttpReadData(hRequest, buf.data(), bytesAvailable, &bytesRead))break;

			sseBuffer.append(buf.data(), bytesRead);

			size_t pos;
			while (!done && (pos = sseBuffer.find('\n')) != std::string::npos)
			{
				std::string line = sseBuffer.substr(0, pos);
				sseBuffer.erase(0, pos + 1);
				if (!line.empty() && line.back() == '\r')line.pop_back();

				const std::string prefix = "data: ";
				if (line.rfind(prefix, 0) != 0)continue; // "data: "で始まらない行(空行等)は無視

				std::string jsonPart = line.substr(prefix.size());
				if (jsonPart == "[DONE]") { done = true; break; }
				if (jsonPart.empty())continue;

				try
				{
					nlohmann::json chunk = nlohmann::json::parse(jsonPart);
					auto& delta = chunk["choices"][0]["delta"];

					// reasoning_content(思考過程)もcontent(本回答)と同じ扱いでストリーミングする。
					// これを無視すると、思考が終わってcontentが出始めるまでの間、画面が
					// 何も動かないように見えるため(生成が遅いのではなく、見えていないだけ)。
					std::string deltaText;
					if (delta.contains("reasoning_content") && delta["reasoning_content"].is_string())
					{
						deltaText += delta["reasoning_content"].get<std::string>();
					}
					if (delta.contains("content") && delta["content"].is_string())
					{
						deltaText += delta["content"].get<std::string>();
					}

					if (!deltaText.empty())
					{
						// 1チャンク=1トークンとして速度計測(1つ目の到着から最後の到着までの区間で計算)
						auto now = std::chrono::steady_clock::now();
						if (!hasFirstToken)
						{
							firstTokenTime = now;
							hasFirstToken = true;
						}
						lastTokenTime = now;
						tokenCount++;

						pendingUtf8 += deltaText;

						size_t incomplete = Helper::InCompleteTailLen(pendingUtf8);
						size_t ready = pendingUtf8.size() - incomplete;
						if (ready > 0)
						{
							std::string readyText = pendingUtf8.substr(0, ready);
							fullResponse += readyText;
							if (onDelta)onDelta(readyText);
							pendingUtf8.erase(0, ready);
						}
					}
				}
				catch (const std::exception&) { /* 不完全なチャンクはスキップ */ }
			}
		} while (bytesAvailable > 0 && !done);

		if (outStats)
		{
			outStats->tokenCount = tokenCount;
			if (tokenCount >= 2) // 1トークンだけだと区間が定義できないので0のままにする
			{
				outStats->elapsedSeconds = std::chrono::duration<double>(lastTokenTime - firstTokenTime).count();
				outStats->tokensPerSecond = (outStats->elapsedSeconds > 0.0)
					? (double)(tokenCount - 1) / outStats->elapsedSeconds
					: 0.0;
			}
		}

		if (!pendingUtf8.empty()) // 最後まで完成しなかった端数は諦めてそのまま流す
		{
			fullResponse += pendingUtf8;
			if (onDelta)onDelta(pendingUtf8);
		}
	}
	else if (!sent)
	{
		fullResponse = "Error: WinHttpSendRequest failed, GetLastError=" + std::to_string(sendError);
		if (onDelta)onDelta(fullResponse);
	}
	else
	{
		fullResponse = "Error: WinHttpReceiveResponse failed, GetLastError=" + std::to_string(receiveError);
		if (onDelta)onDelta(fullResponse);
	}

	WinHttpCloseHandle(hRequest);
	WinHttpCloseHandle(hConnect);
	WinHttpCloseHandle(hSession);
	return fullResponse;
}
