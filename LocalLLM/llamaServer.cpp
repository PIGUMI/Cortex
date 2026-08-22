#include "llamaServer.h"

#include <Windows.h>
#include <winhttp.h>
#include <nlohmann/json.hpp>
#include <vector>
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
