/*
* テンプレート
* C/C++を使用したWindowsアプリケーションの基本的なテンプレート
* ISO C++20 準拠
* Application/main.cpp
* 最終更新日: 2026/08/16
*/

#include "Window.h"
#include "DirectX12.h"
#include "Helper.h"
#include "llamaServer.h"
#include "Worker.h"
#include <mutex>


// Windowsアプリケーションのエントリーポイント
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	Window* window = Window::GetInstance();
	DirectX::DirectX12* directX12 = DirectX::DirectX12::Get();
	DirectX::Descriptor* descriptor = DirectX::Descriptor::Get();

	/* 初期化 */
	{
		/* Windowの初期化 */
		window->SetWindowTitle("Template");
		window->SetWindowWidth(720);
		window->SetWindowHeight(480);
		window->Initialize(hInstance, nCmdShow);

		HWND Sub = window->CreateSubSeparateWindow(1, "Sub Window", 1000, 100, 400, 900);
		window->CreateTextBox(2, "", 5, 10, 370, 700, Sub);
		window->CreateRichEdit(3, "", 5, 720, 300, 100, Sub);
		window->CreateButton(4, "Send", 310, 720, 60, 100, Sub);

		/* DirectX12の初期化 */
		directX12->Init(window->GetMainWindowHandle(), window->GetClientWidth(), window->GetClientHeight());

		/* Descriptorの初期化 */
		descriptor->Init();
	}

	/* ループ処理 */
	{
		std::mutex llmResultMutex;
		std::string m_llmResult = "";
		size_t displayedLength = 0; // m_llmResultのうち、既に画面に表示済みのバイト数
		Worker* worker = nullptr;
		bool aiPrefixShown = false;  // "AI: "を表示済みか
		bool resultDisplayed = true; // 末尾の改行を表示済みか
		LocalLLM::StreamStats lastStats; // 直近の生成速度(完了後にのみ読む。worker->IsFinished()のatomic同期に相乗りするのでmutex不要)

		/* 基礎ループ */
		MSG msg = {};

		bool isRunning = false;

		try
		{
			while (true)
			{
				if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
				{
					if (msg.message == WM_QUIT)
					{
						break;
					}
					TranslateMessage(&msg);
					DispatchMessageA(&msg);
				}
				else
				{
					// 更新処理

					if (window->IsButtonClicked(4) && (worker == nullptr || worker->IsFinished()))
					{
						window->AddTextBoxText(2, "You: " + window->GetEditText(3) + "\r\n\r\n");
				
						delete worker; // 前回分の後始末(joinable済みなのですぐ壊せる)

						std::string ansiInput = window->GetEditText(3);
						std::string utf8Input = Helper::AnsiToUtf8(ansiInput);
						window->SetBoxText(3, ""); // 入力欄をクリア

						{
							std::lock_guard<std::mutex> lock(llmResultMutex);
							m_llmResult.clear();
						}
						displayedLength = 0;
						aiPrefixShown = false;
						resultDisplayed = false;

						worker = new Worker([&m_llmResult, &llmResultMutex, &lastStats, utf8Input]()
							{
								LocalLLM::CallLlamaServerStream(utf8Input, [&m_llmResult, &llmResultMutex](const std::string& delta)
									{
										std::lock_guard<std::mutex> lock(llmResultMutex);
										m_llmResult += delta;
									}, &lastStats);
							});
					}

					// 毎フレーム: 前回チェック時から新しく届いた分だけ切り出して表示する
					if (worker)
					{
						std::string newUtf8;
						{
							std::lock_guard<std::mutex> lock(llmResultMutex);
							if (m_llmResult.size() > displayedLength)
							{
								newUtf8 = m_llmResult.substr(displayedLength);
								displayedLength = m_llmResult.size();
							}
						}

						if (!newUtf8.empty())
						{
							if (!aiPrefixShown)
							{
								window->AddTextBoxText(2, "AI: ");
								aiPrefixShown = true;
							}
							window->AddTextBoxText(2, Helper::Utf8ToAnsi(newUtf8));
						}

						if (worker->IsFinished() && !resultDisplayed)
						{
							char speedText[64];
							sprintf_s(speedText, "(%.1f tok/s)", lastStats.tokensPerSecond);
							window->AddTextBoxText(2, std::string(speedText) + "\r\n\r\n");
							resultDisplayed = true;
						}
					}



					// 描画処理
					directX12->BeginDraw();

					// ここに描画処理を追加する

					directX12->EndDraw();
					directX12->ApplyResizeIfNeeded();

				}
			}
		}
		catch (const std::exception& e)
		{
			MessageBoxA(nullptr, e.what(), "Error", MB_OK | MB_ICONERROR);
		}
		catch (...)
		{
			MessageBoxA(nullptr, "Unknown error occurred.", "Error", MB_OK | MB_ICONERROR);
		}
	}

	/* 終了処理 */
	{
		descriptor->Del();
		directX12->Del();
		window->DestroyInstance();
	}

	return 0;
}
