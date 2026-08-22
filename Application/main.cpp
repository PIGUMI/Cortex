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

					if (window->IsButtonClicked(4))
					{
						std::string ansiInput = window->GetEditText(3);

						std::string utf8Input = Helper::AnsiToUtf8(ansiInput);// UTF-8へ変換

						std::string utf8Response = LocalLLM::CallLlamaServer(utf8Input);
						std::string ansiResponse = Helper::Utf8ToAnsi(utf8Response);// ANSIへ変換

						window->AddTextBoxText(2, "You: " + ansiInput + "\r\n");
						window->AddTextBoxText(2, "AI: " + ansiResponse + "\r\n\r\n");
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
