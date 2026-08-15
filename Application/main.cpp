/*
* テンプレート
* C/C++を使用したWindowsアプリケーションの基本的なテンプレート
* ISO C++20 準拠
* Application/main.cpp
* 最終更新日: 2026/08/16
*/

#include "Window.h"
#include "DirectX12.h"

// Windowsアプリケーションのエントリーポイント
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	/* Windowの初期化 */
	Window* window = Window::GetInstance();
	window->SetWindowTitle("Template");
	window->SetWindowWidth(720);
	window->SetWindowHeight(480);
	window->Initialize(hInstance, nCmdShow);

	window->CreateSubSeparateWindow(1, "Sub Window", 100, 100, 400, 300);


	/* DirectX12の初期化 */
	DirectX::DirectX12* directX12 = DirectX::DirectX12::Get();
	directX12->Init(window->GetMainWindowHandle(), window->GetClientWidth(), window->GetClientHeight());

	/* Descriptorの初期化 */
	DirectX::Descriptor* descriptor = DirectX::Descriptor::Get();
	descriptor->Init();


	/* 基礎ループ */
	MSG msg = {};

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
