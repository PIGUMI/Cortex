#include <windows.h>
#include "Template.h"

// Windowsアプリケーションのエントリーポイント
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	return TemplateMain(hInstance, nCmdShow);
}
