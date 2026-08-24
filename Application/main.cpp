#include <windows.h>
#include "Template.h"

#include "Window.h"

// Windowsアプリケーションのエントリーポイント
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	TemplateMain(hInstance, nCmdShow);

	return 0;
}
