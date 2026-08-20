/*
* テンプレート
* C/C++を使用したWindowsアプリケーションの基本的なテンプレート
* ISO C++20 準拠
* Application/main.cpp
* 最終更新日: 2026/08/16
*/

#include "Window.h"
#include "DirectX12.h"
#include "BaseLLM.h"

// Windowsアプリケーションのエントリーポイント
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	/* Windowの初期化 */
	Window* window = Window::GetInstance();
	window->SetWindowTitle("Template");
	window->SetWindowWidth(720);
	window->SetWindowHeight(480);
	window->Initialize(hInstance, nCmdShow);

	HWND Sub = window->CreateSubSeparateWindow(1, "Sub Window", 1000, 100, 400, 900);
	window->CreateTextBox(2, "", 5, 10, 370, 700, Sub);
	window->CreateRichEdit(3, "", 5, 720, 300, 100, Sub);
	window->CreateButton(4, "Send", 310, 720, 60, 100, Sub);


	/* DirectX12の初期化 */
	DirectX::DirectX12* directX12 = DirectX::DirectX12::Get();
	directX12->Init(window->GetMainWindowHandle(), window->GetClientWidth(), window->GetClientHeight());

	/* Descriptorの初期化 */
	DirectX::Descriptor* descriptor = DirectX::Descriptor::Get();
	descriptor->Init();


	BaseLLM* llm = new BaseLLM("Model/Qwen3.5-4B-UD-Q4_K_XL.gguf", nullptr, 99, 4096, 512, 1024);
	

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

				if(window->IsButtonClicked(4))
				{
					std::string prompt = window->GetEditText(3);
					window->SetBoxText(3, "");
					window->AddTextBoxText(2, "User: " + prompt + "\r\n");

					window->AddTextBoxText(2, "LLM: " + Helper::Utf8ToAnsi(llm->ProcessPrompt(Helper::AnsiToUtf8(prompt))) + "\r\n");
					isRunning = true;
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

	descriptor->Del();
	directX12->Del();
	window->DestroyInstance();
	return 0;
}
