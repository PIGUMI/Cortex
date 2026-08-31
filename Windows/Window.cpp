#include "Window.h"
using namespace std;
#include <Windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <set>
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif
#ifndef DWMSBT_MAINWINDOW
#define DWMSBT_MAINWINDOW 2
#endif

namespace {
	// レジストリのAppsUseLightThemeを見てOSがダークテーマかどうか判定
	bool IsSystemInDarkMode()
	{
		HKEY hKey;
		if (RegOpenKeyExA(HKEY_CURRENT_USER,
			"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
			0, KEY_READ, &hKey) != ERROR_SUCCESS) {
			return false;
		}
		DWORD value = 1;
		DWORD size = sizeof(value);
		LONG result = RegQueryValueExA(hKey, "AppsUseLightTheme", nullptr, nullptr, (LPBYTE)&value, &size);
		RegCloseKey(hKey);
		return result == ERROR_SUCCESS && value == 0;
	}
}

//extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

/* ウインドウプロシージャ */
LRESULT WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	//if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))return true;

	switch (message) {
	case WM_COMMAND:
	{
		int id = LOWORD(wParam);
		int code = HIWORD(wParam);
		if (code == BN_CLICKED) {
			switch (id) {
			case ID_TITLEBAR_MIN:
				ShowWindow(hWnd, SW_MINIMIZE);
				return 0;
			case ID_TITLEBAR_MAXRESTORE:
				ShowWindow(hWnd, IsZoomed(hWnd) ? SW_RESTORE : SW_MAXIMIZE);
				return 0;
			case ID_TITLEBAR_CLOSE:
				PostMessage(hWnd, WM_CLOSE, 0, 0);
				return 0;
			case ID_TITLEBAR_FILE:
			case ID_TITLEBAR_EDIT:
				Window::GetInstance()->ShowTitleBarMenu(id);
				return 0;
			default:
				Window::GetInstance()->SetButtonClicked(id);
			}
		}
		else if (code == EN_KILLFOCUS) {
			Window::GetInstance()->ShowRichEditPlaceholder(id);
		}
		else if (code == EN_SETFOCUS) {
			Window::GetInstance()->HideRichEditPlaceholder(id);
		}
		break;
	}
	case WM_DRAWITEM:
	{
		LPDRAWITEMSTRUCT di = (LPDRAWITEMSTRUCT)lParam;
		if (di->CtlType == ODT_BUTTON) {
			Window::GetInstance()->DrawFlatButton(di);
			return TRUE;
		}
		if (di->CtlType == ODT_TAB) {
			Window::GetInstance()->DrawTabItem(di);
			return TRUE;
		}
		break;
	}
	case WM_NOTIFY:
	{
		NMHDR* nmhdr = (NMHDR*)lParam;
		if (!nmhdr) break;
		if (Window::GetInstance()->m_TabShowWindow.count((const int)nmhdr->idFrom) == 0) break;
		if (nmhdr->code == TCN_SELCHANGE) {
			Window::GetInstance()->ShowOrHideTabControls((const int)nmhdr->idFrom, TRUE);
		}
		break;
	}
	case WM_CTLCOLORSTATIC:
	case WM_CTLCOLOREDIT:
	case WM_CTLCOLORLISTBOX:
	{
		int id = GetDlgCtrlID((HWND)lParam);
		HBRUSH hBrush = Window::GetInstance()->HandleCtlColor(id, (HDC)wParam);
		if (hBrush) return (LRESULT)hBrush;
		break;
	}
	// 自作タイトルバーにする都合上、OSに「タイトルバー＋枠の分だけ内側にクライアント領域を作る」
	// 処理をさせないようにする。TRUEのときに何もせず0を返すと、ウインドウ全体がクライアント領域になる。
	case WM_NCCALCSIZE:
	{
		if (wParam == TRUE && hWnd == Window::GetInstance()->GetMainWindowHandle()) {
			NCCALCSIZE_PARAMS* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
			WINDOWPLACEMENT wp{};
			wp.length = sizeof(wp);
			GetWindowPlacement(hWnd, &wp);
			if (wp.showCmd == SW_SHOWMAXIMIZED) {
				// 最大化時はモニタの作業領域からはみ出さないよう、見えない枠の分だけ内側に詰める
				int frameX = GetSystemMetrics(SM_CXFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
				int frameY = GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
				params->rgrc[0].left += frameX;
				params->rgrc[0].top += frameY;
				params->rgrc[0].right -= frameX;
				params->rgrc[0].bottom -= frameY;
			}
			return 0;
		}
		break;
	}
	// タイトルバーの見た目を消した分、「上端をつかんでドラッグ移動」「端をつかんでリサイズ」
	// という当たり判定をここで自前判定して復元する。
	case WM_NCHITTEST:
	{
		if (hWnd != Window::GetInstance()->GetMainWindowHandle()) break;

		POINT ptScreen = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		RECT rcWindow;
		GetWindowRect(hWnd, &rcWindow);

		const int resizeBorder = GetSystemMetrics(SM_CXFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
		if (!IsZoomed(hWnd)) {
			bool onLeft = ptScreen.x < rcWindow.left + resizeBorder;
			bool onRight = ptScreen.x >= rcWindow.right - resizeBorder;
			bool onTop = ptScreen.y < rcWindow.top + resizeBorder;
			bool onBottom = ptScreen.y >= rcWindow.bottom - resizeBorder;

			if (onTop && onLeft) return HTTOPLEFT;
			if (onTop && onRight) return HTTOPRIGHT;
			if (onBottom && onLeft) return HTBOTTOMLEFT;
			if (onBottom && onRight) return HTBOTTOMRIGHT;
			if (onLeft) return HTLEFT;
			if (onRight) return HTRIGHT;
			if (onTop) return HTTOP;
			if (onBottom) return HTBOTTOM;
		}

		POINT ptClient = ptScreen;
		ScreenToClient(hWnd, &ptClient);
		if (ptClient.y >= 0 && ptClient.y < Window::GetInstance()->GetTitleBarHeight()) {
			HWND hChild = ChildWindowFromPointEx(hWnd, ptClient, CWP_SKIPINVISIBLE | CWP_SKIPDISABLED | CWP_SKIPTRANSPARENT);
			if (hChild != nullptr && hChild != hWnd) {
				return HTCLIENT; // 自作タイトルバー上のボタンは通常のクリックとして扱う
			}
			return HTCAPTION; // ボタン以外の帯部分はドラッグ移動 + ダブルクリックで最大化
		}

		return HTCLIENT;
	}
	// タイトルバーを消したことでOSが行う非クライアント領域の再描画(ちらつきの原因)を抑止する
	case WM_NCACTIVATE:
		if (hWnd == Window::GetInstance()->GetMainWindowHandle()) {
			return DefWindowProc(hWnd, message, wParam, -1);
		}
		break;
	case WM_SIZE:
		if (wParam != SIZE_MINIMIZED)
		{
			UINT w = LOWORD(lParam);
			UINT h = HIWORD(lParam);
			//DirectX::DirectX12::Get()->RequestResize(w, h);
			if (hWnd == Window::GetInstance()->GetMainWindowHandle()) {
				Window::GetInstance()->LayoutTitleBarButtons();
				Window::GetInstance()->ApplyProportionalLayout();
				Window::GetInstance()->NotifyResize((int)w, (int)h);
			}
		}
		return 0;
	case WM_DESTROY:
		if (hWnd == Window::GetInstance()->GetMainWindowHandle()) {
			for (const auto& pair : Window::GetInstance()->GetChildWindows()) {
				if (pair.second && IsWindow(pair.second)) {
					DestroyWindow(pair.second);
				}
			}
			PostQuitMessage(0);
		}
		return DefWindowProc(hWnd, message, wParam, lParam);

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}
/* ~~~~~~~~~~~~~~~~~~~~~~ */

/* Static定義 */
Window* Window::m_instance = nullptr;

Window* Window::GetInstance()
{
	if (m_instance == nullptr) {
		m_instance = new Window();
		m_instance->m_wcex = {};
		m_instance->m_wcex.cbSize = sizeof(WNDCLASSEX);
		m_instance->m_wcex.style = CS_OWNDC | CS_VREDRAW | CS_HREDRAW;
		m_instance->m_wcex.lpszClassName = m_instance->m_windowTitle.c_str();
		m_instance->m_wcex.lpfnWndProc = WndProc;
		m_instance->m_wcex.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
		m_instance->m_wcex.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
		m_instance->m_wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
		m_instance->m_wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		m_instance->rect = { 0, 0, m_instance->m_windowWidth, m_instance->m_windowHeight };
		m_instance->m_TabShowWindow = {};
	}
	return m_instance;
}

void Window::DestroyInstance()
{
	if (m_instance != nullptr) {
		// すべての子ウインドウを破棄
		for (const auto& pair : m_instance->m_childWindows) {
			if (pair.second) {
				DestroyWindow(pair.second);
			}
		}
		// メインウインドウを破棄
		if (m_instance->m_hWnd) {
			DestroyWindow(m_instance->m_hWnd);
		}
		if (m_instance->m_defaultFont) {
			DeleteObject(m_instance->m_defaultFont);
		}
		for (auto& pair : m_instance->m_controlColors) {
			if (pair.second.backgroundBrush) DeleteObject(pair.second.backgroundBrush);
		}
		CoUninitialize();
		// インスタンスを破棄
		delete m_instance;
		m_instance = nullptr;
	}
}

bool Window::Initialize(HINSTANCE hInstance, int nCmdShow)
{
	m_wcex.hInstance = hInstance;
	if (RegisterClassEx(&m_wcex) == 0) {
		MessageBox(nullptr, "ウインドウクラスの登録に失敗", "エラー", MB_OK | MB_ICONERROR);
		return false;
	}
	rect = { 0, 0, m_windowWidth, m_windowHeight };
	AdjustWindowRectEx(&rect, style, FALSE, exStyle);
	m_hWnd = CreateWindowEx(
		exStyle,
		m_wcex.lpszClassName,
		m_wcex.lpszClassName,
		style,
		CW_USEDEFAULT, CW_USEDEFAULT,
		rect.right - rect.left, rect.bottom - rect.top,
		nullptr, nullptr, hInstance, nullptr
	);
	if (m_hWnd == nullptr) {
		MessageBox(nullptr, "ウインドウの作成に失敗", "エラー", MB_OK | MB_ICONERROR);
		return false;
	}
	ApplyModernWindowStyle(m_hWnd);
	CreateTitleBar();
	ShowWindow(m_hWnd, nCmdShow);
	UpdateWindow(m_hWnd);
	CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	return true;
}

void Window::ApplyModernWindowStyle(HWND hwnd)
{
	// タイトルバーをOSのダーク/ライト設定に合わせる
	BOOL useDarkMode = IsSystemInDarkMode() ? TRUE : FALSE;
	DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode));

	// Windows 11: Micaの背景素材 + ウインドウの角丸
	DWORD backdrop = DWMSBT_MAINWINDOW;
	DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));

	DWORD cornerPreference = DWMWCP_ROUND;
	DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPreference, sizeof(cornerPreference));

	// 未対応OS(Windows 10以前)ではDwmSetWindowAttributeが静かに失敗するだけなので、
	// エラーチェックは行わずベストエフォートで適用する。

	// タイトルバーを自前描画にする(WM_NCCALCSIZEで非クライアント領域を0にする)と、
	// 何もしないとDWMの影が付かなくなる。下端を1pxだけ非クライアント扱いにする定石で影を復元する。
	MARGINS margins = { 0, 0, 0, 1 };
	DwmExtendFrameIntoClientArea(hwnd, &margins);
}

void Window::CreateTitleBar()
{
	UINT dpi = GetDpiForWindow(m_hWnd);
	m_titleBarHeight = MulDiv(36, (int)dpi, 96);
	int menuButtonWidth = MulDiv(60, (int)dpi, 96);
	int margin = MulDiv(4, (int)dpi, 96);

	CreateButton(ID_TITLEBAR_FILE, "ファイル", margin, margin, menuButtonWidth, m_titleBarHeight - margin * 2,nullptr,true);
	CreateButton(ID_TITLEBAR_EDIT, "編集", margin + menuButtonWidth, margin, menuButtonWidth, m_titleBarHeight - margin * 2,nullptr,true);

	CreateButton(ID_TITLEBAR_MIN, "－", 0, 0, 1, 1,nullptr,true);
	CreateButton(ID_TITLEBAR_MAXRESTORE, "□", 0, 0, 1, 1,nullptr,true);
	CreateButton(ID_TITLEBAR_CLOSE, "×", 0, 0, 1, 1,nullptr,true);

	LayoutTitleBarButtons();
}

void Window::LayoutTitleBarButtons()
{
	HWND hMin = GetChildWindowHandle(ID_TITLEBAR_MIN);
	HWND hMaxRestore = GetChildWindowHandle(ID_TITLEBAR_MAXRESTORE);
	HWND hClose = GetChildWindowHandle(ID_TITLEBAR_CLOSE);
	if (!hMin || !hMaxRestore || !hClose) return;

	UINT dpi = GetDpiForWindow(m_hWnd);
	int buttonWidth = MulDiv(46, (int)dpi, 96);
	int clientWidth = GetClientWidth();

	int x = clientWidth - buttonWidth;
	SetWindowPos(hClose, nullptr, x, 0, buttonWidth, m_titleBarHeight, SWP_NOZORDER | SWP_NOACTIVATE);
	x -= buttonWidth;
	SetWindowPos(hMaxRestore, nullptr, x, 0, buttonWidth, m_titleBarHeight, SWP_NOZORDER | SWP_NOACTIVATE);
	x -= buttonWidth;
	SetWindowPos(hMin, nullptr, x, 0, buttonWidth, m_titleBarHeight, SWP_NOZORDER | SWP_NOACTIVATE);
}

void Window::ShowTitleBarMenu(int buttonId)
{
	HWND hButton = GetChildWindowHandle(buttonId);
	if (!hButton) return;

	HMENU hMenu = CreatePopupMenu();
	if (buttonId == ID_TITLEBAR_FILE) {
		AppendMenuA(hMenu, MF_STRING, 1, "新規");
		AppendMenuA(hMenu, MF_STRING, 2, "開く");
		AppendMenuA(hMenu, MF_SEPARATOR, 0, nullptr);
		AppendMenuA(hMenu, MF_STRING, 3, "終了");
	} else {
		AppendMenuA(hMenu, MF_STRING, 1, "元に戻す");
		AppendMenuA(hMenu, MF_STRING, 2, "切り取り");
		AppendMenuA(hMenu, MF_STRING, 3, "コピー");
		AppendMenuA(hMenu, MF_STRING, 4, "貼り付け");
	}

	RECT rc;
	GetWindowRect(hButton, &rc);
	TrackPopupMenuEx(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN, rc.left, rc.bottom, m_hWnd, nullptr);
	DestroyMenu(hMenu);
}

void Window::DrawFlatButton(LPDRAWITEMSTRUCT di)
{
	bool pressed = (di->itemState & ODS_SELECTED) != 0;

	COLORREF bgColor = pressed ? RGB(225, 225, 225) : RGB(255, 255, 255);
	COLORREF textColor = RGB(32, 32, 32);

	auto it = m_controlColors.find((int)di->CtlID);
	if (it != m_controlColors.end()) {
		textColor = it->second.textColor;
		if (pressed) {
			int r = max(0, GetRValue(it->second.backgroundColor) - 24);
			int g = max(0, GetGValue(it->second.backgroundColor) - 24);
			int b = max(0, GetBValue(it->second.backgroundColor) - 24);
			bgColor = RGB(r, g, b);
		}
		else {
			bgColor = it->second.backgroundColor;
		}
	}

	HBRUSH hBrush = CreateSolidBrush(bgColor);
	FillRect(di->hDC, &di->rcItem, hBrush);
	DeleteObject(hBrush);

	char text[256] = {};
	GetWindowTextA(di->hwndItem, text, sizeof(text));

	SetBkMode(di->hDC, TRANSPARENT);
	SetTextColor(di->hDC, textColor);
	SelectObject(di->hDC, GetDefaultFont());
	DrawTextA(di->hDC, text, -1, &di->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void Window::SetControlColor(int id, COLORREF textColor, COLORREF backgroundColor)
{
	ControlColor& entry = m_controlColors[id];
	if (entry.backgroundBrush) {
		DeleteObject(entry.backgroundBrush);
	}
	entry.textColor = textColor;
	entry.backgroundColor = backgroundColor;
	entry.backgroundBrush = CreateSolidBrush(backgroundColor);

	HWND hControl = GetChildWindowHandle(id);
	if (hControl) {
		InvalidateRect(hControl, nullptr, TRUE);
	}
}

HBRUSH Window::HandleCtlColor(int id, HDC hdc)
{
	auto it = m_controlColors.find(id);
	if (it == m_controlColors.end()) return nullptr;

	SetTextColor(hdc, it->second.textColor);
	SetBkColor(hdc, it->second.backgroundColor);
	return it->second.backgroundBrush;
}

void Window::DrawTabItem(LPDRAWITEMSTRUCT di)
{
	int id = (int)di->CtlID;
	auto it = m_controlColors.find(id);
	COLORREF bgColor = (it != m_controlColors.end()) ? it->second.backgroundColor : RGB(240, 240, 240);
	COLORREF textColor = (it != m_controlColors.end()) ? it->second.textColor : RGB(0, 0, 0);

	HBRUSH hBrush = CreateSolidBrush(bgColor);
	FillRect(di->hDC, &di->rcItem, hBrush);
	DeleteObject(hBrush);

	bool selected = (di->itemState & ODS_SELECTED) != 0;
	if (selected) {
		HPEN hPen = CreatePen(PS_SOLID, 2, textColor);
		HGDIOBJ old = SelectObject(di->hDC, hPen);
		MoveToEx(di->hDC, di->rcItem.left, di->rcItem.bottom - 1, nullptr);
		LineTo(di->hDC, di->rcItem.right, di->rcItem.bottom - 1);
		SelectObject(di->hDC, old);
		DeleteObject(hPen);
	}

	char text[256] = {};
	TCITEMA tci = {};
	tci.mask = TCIF_TEXT;
	tci.pszText = text;
	tci.cchTextMax = sizeof(text);
	SendMessageA(di->hwndItem, TCM_GETITEMA, di->itemID, (LPARAM)&tci);

	SetBkMode(di->hDC, TRANSPARENT);
	SetTextColor(di->hDC, textColor);
	SelectObject(di->hDC, GetDefaultFont());
	DrawTextA(di->hDC, text, -1, &di->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void Window::RegisterControlLayout(int id, HWND hControl)
{
	if (!hControl) return;
	if (id >= ID_TITLEBAR_FILE && id <= ID_TITLEBAR_CLOSE) return; // タイトルバーのボタンは専用のLayoutTitleBarButtonsで管理するため対象外

	HWND hParent = GetParent(hControl);
	if (!hParent) return;

	RECT rcParent;
	GetClientRect(hParent, &rcParent);
	double parentWidth = (double)(rcParent.right - rcParent.left);
	double parentHeight = (double)(rcParent.bottom - rcParent.top);
	if (parentWidth <= 0 || parentHeight <= 0) return;

	RECT rcControl;
	GetWindowRect(hControl, &rcControl);
	POINT topLeft = { rcControl.left, rcControl.top };
	POINT bottomRight = { rcControl.right, rcControl.bottom };
	ScreenToClient(hParent, &topLeft);
	ScreenToClient(hParent, &bottomRight);

	ControlLayout layout{};
	layout.id = id;
	layout.fracX = topLeft.x / parentWidth;
	layout.fracY = topLeft.y / parentHeight;
	layout.fracWidth = (bottomRight.x - topLeft.x) / parentWidth;
	layout.fracHeight = (bottomRight.y - topLeft.y) / parentHeight;
	m_controlLayouts.push_back(layout);
}

void Window::ApplyProportionalLayout()
{
	for (const auto& layout : m_controlLayouts) {
		HWND hControl = GetChildWindowHandle(layout.id);
		if (!hControl) continue;
		HWND hParent = GetParent(hControl);
		if (!hParent) continue;

		RECT rcParent;
		GetClientRect(hParent, &rcParent);
		int parentWidth = rcParent.right - rcParent.left;
		int parentHeight = rcParent.bottom - rcParent.top;

		int x = (int)(layout.fracX * parentWidth);
		int y = (int)(layout.fracY * parentHeight);
		int width = (int)(layout.fracWidth * parentWidth);
		int height = (int)(layout.fracHeight * parentHeight);

		SetWindowPos(hControl, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);

		char className[64] = {};
		GetClassNameA(hControl, className, sizeof(className));
		if (strcmp(className, WC_TABCONTROL) == 0) {
			AutoSizeTabItems(layout.id);
		}
	}
}

HFONT Window::GetDefaultFont()
{
	if (m_defaultFont != nullptr) return m_defaultFont;

	UINT dpi = m_hWnd ? GetDpiForWindow(m_hWnd) : 96;
	int pointSize = 9;
	int fontHeight = -MulDiv(pointSize, (int)dpi, 72);

	m_defaultFont = CreateFontA(
		fontHeight, 0, 0, 0,
		FW_NORMAL, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
		"Segoe UI"
	);
	return m_defaultFont;
}

HWND Window::CreateButton(int id, const string& text, int x, int y, int width, int height, HWND hwnd,bool flat)
{
	if (hwnd == nullptr) hwnd = m_hWnd;
	DWORD buttonStyle = WS_CHILD | WS_VISIBLE | (flat ? BS_OWNERDRAW : BS_PUSHBUTTON);
	HWND hButton = CreateWindowEx(
		0,
		"BUTTON",
		text.c_str(),
		buttonStyle,
		x, y, width, height,
		hwnd,
		(HMENU)(intptr_t)id,
		m_wcex.hInstance,
		NULL
	);
	if (hButton == nullptr) {
		MessageBox(nullptr, "ボタンの作成に失敗", "エラー", MB_OK | MB_ICONERROR);
		return nullptr;
	}
	SendMessage(hButton, WM_SETFONT, (WPARAM)GetDefaultFont(), TRUE);
	m_childWindows[id] = hButton;
	RegisterControlLayout(id, hButton);
	ShowWindow(hButton, SW_SHOW);
	UpdateWindow(hButton);
	return hButton;
}

HWND Window::CreateEdit(int id, const string& text, int x, int y, int width, int height)
{
	HWND hEdit = CreateWindowEx(
		0,
		"EDIT",
		text.c_str(),
		WS_CHILD | WS_VISIBLE | WS_BORDER,
		x, y, width, height,
		m_hWnd,
		(HMENU)(intptr_t)id,
		m_wcex.hInstance,
		NULL
	);
	if (hEdit == nullptr) {
		MessageBox(nullptr, "編集ボックスの作成に失敗", "エラー", MB_OK | MB_ICONERROR);
		return nullptr;
	}
	SendMessage(hEdit, WM_SETFONT, (WPARAM)GetDefaultFont(), TRUE);
	m_childWindows[id] = hEdit;
	RegisterControlLayout(id, hEdit);
	ShowWindow(hEdit, SW_SHOW);
	UpdateWindow(hEdit);
	return hEdit;
}

HWND Window::CreateTextBox(int id, const string& text, int x, int y, int width, int height, HWND hwnd)
{
	if (hwnd == nullptr) hwnd = m_hWnd;
	HWND hTextBox = CreateWindowEx(
		0,
		"EDIT",
		text.c_str(),
		WS_CHILD | WS_VISIBLE | WS_BORDER | ES_READONLY | ES_MULTILINE | ES_AUTOVSCROLL,
		x, y, width, height,
		hwnd,
		(HMENU)(intptr_t)id,
		m_wcex.hInstance,
		NULL
	);
	if (hTextBox == nullptr) {
		MessageBox(nullptr, "テキストボックスの作成に失敗", "エラー", MB_OK | MB_ICONERROR);
		return nullptr;
	}
	SendMessage(hTextBox, WM_SETFONT, (WPARAM)GetDefaultFont(), TRUE);
	m_childWindows[id] = hTextBox;
	RegisterControlLayout(id, hTextBox);
	ShowWindow(hTextBox, SW_SHOW);
	UpdateWindow(hTextBox);
	return hTextBox;
}

void Window::AddTextBoxText(int id, const string& text)
{
	HWND hTextBox = GetChildWindowHandle(id);
	if (!hTextBox) {
		MessageBox(nullptr, "TextBoxが見つかりません", "エラー", MB_OK | MB_ICONERROR);
		return;
	}

	string converted = text;
	size_t pos = 0;
	while ((pos = converted.find('\n', pos)) != string::npos) {
		converted.replace(pos, 1, "\r\n");
		pos += 2;
	}

	int textLength = GetWindowTextLength(hTextBox);
	SendMessage(hTextBox, EM_SETSEL, (WPARAM)textLength, (LPARAM)textLength);
	SendMessage(hTextBox, EM_REPLACESEL, FALSE, (LPARAM)converted.c_str());
}

HWND Window::CreateListBox(int id, const vector<string>& items, int x, int y, int width, int height)
{
	HWND hListBox = CreateWindowEx(
		0,
		"LISTBOX",
		nullptr,
		WS_CHILD | WS_VISIBLE | WS_BORDER | LBS_NOTIFY,
		x, y, width, height,
		m_hWnd,
		(HMENU)(intptr_t)id,
		m_wcex.hInstance,
		NULL
	);
	if (hListBox == nullptr) {
		MessageBox(nullptr, "リストボックスの作成に失敗", "エラー", MB_OK | MB_ICONERROR);
		return nullptr;
	}
	SendMessage(hListBox, WM_SETFONT, (WPARAM)GetDefaultFont(), TRUE);
	for (const auto& item : items) {
		SendMessage(hListBox, LB_ADDSTRING, 0, (LPARAM)item.c_str());
	}
	m_childWindows[id] = hListBox;
	RegisterControlLayout(id, hListBox);
	ShowWindow(hListBox, SW_SHOW);
	UpdateWindow(hListBox);
	return hListBox;
}

void Window::AddListBoxItem(int id, const string& item)
{
	HWND hListBox = GetChildWindowHandle(id);
	if (!hListBox) {
		MessageBox(nullptr, "リストボックスが見つかりません", "エラー", MB_OK | MB_ICONERROR);
		return;
	}
	SendMessage(hListBox, LB_ADDSTRING, 0, (LPARAM)item.c_str());
}

void Window::RemoveListBoxItem(int id, int index)
{
	HWND hListBox = GetChildWindowHandle(id);
	if (!hListBox) {
		MessageBox(nullptr, "リストボックスが見つかりません", "エラー", MB_OK | MB_ICONERROR);
		return;
	}
	SendMessage(hListBox, LB_DELETESTRING, index, 0);
}

HWND Window::CreateComboBox(int id, const vector<string>& items, int x, int y, int width, int height)
{
	HWND hComboBox = CreateWindowEx(
		0,
		"COMBOBOX",
		nullptr,
		WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | CBS_DROPDOWNLIST, // WS_VSCROLL追加、CBS_DROPDOWNLISTに変更
		x, y, width,
		height + 200,  // コントロール高さ + ドロップダウンリスト展開分（100px = 約3件分）
		m_hWnd,
		(HMENU)(intptr_t)id,
		m_wcex.hInstance,
		NULL
	);
	if (hComboBox == nullptr) {
		MessageBox(nullptr, "コンボボックスの作成に失敗", "エラー", MB_OK | MB_ICONERROR);
		return nullptr;
	}
	SendMessage(hComboBox, WM_SETFONT, (WPARAM)GetDefaultFont(), TRUE);
	for (const auto& item : items) {
		SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)item.c_str());
	}
	m_childWindows[id] = hComboBox;
	RegisterControlLayout(id, hComboBox);
	ShowWindow(hComboBox, SW_SHOW);
	UpdateWindow(hComboBox);
	return hComboBox;
}

void Window::AddComboBoxItem(int id, const string& item)
{
	HWND hComboBox = GetChildWindowHandle(id);
	if (!hComboBox) {
		MessageBox(nullptr, "コンボボックスが見つかりません", "エラー", MB_OK | MB_ICONERROR);
		return;
	}
	SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)item.c_str());
}

void Window::RemoveComboBoxItem(int id, int index)
{
	HWND hComboBox = GetChildWindowHandle(id);
	if (!hComboBox) {
		MessageBox(nullptr, "コンボボックスが見つかりません", "エラー", MB_OK | MB_ICONERROR);
		return;
	}
	SendMessage(hComboBox, CB_DELETESTRING, index, 0);
}

HWND Window::CreateScrollBar(int id, int x, int y, int width, int height, bool isVertical)
{
	DWORD style = WS_CHILD | WS_VISIBLE | SBS_SIZEBOX;
	style |= isVertical ? SBS_VERT : SBS_HORZ;
	HWND hScrollBar = CreateWindowEx(
		0,
		"SCROLLBAR",
		nullptr,
		style,
		x, y, width, height,
		m_hWnd,
		(HMENU)(intptr_t)id,
		m_wcex.hInstance,
		NULL
	);
	if (hScrollBar == nullptr) {
		MessageBox(nullptr, "スクロールバーの作成に失敗", "エラー", MB_OK | MB_ICONERROR);
		return nullptr;
	}
	m_childWindows[id] = hScrollBar;
	RegisterControlLayout(id, hScrollBar);
	ShowWindow(hScrollBar, SW_SHOW);
	UpdateWindow(hScrollBar);
	return hScrollBar;
}

HWND Window::CreateRichEdit(int id, const string& text, int x, int y, int width, int height, HWND hwnd)
{
	static HINSTANCE hRichEditLib = LoadLibraryA("Msftedit.dll");
	if (!hRichEditLib) {
		MessageBox(nullptr, "リッチエディタDLLのロードに失敗", "エラー", MB_OK | MB_ICONERROR);
		return nullptr;
	}

	if (hwnd == nullptr)hwnd = m_hWnd;

	HWND hRichEdit = CreateWindowEx(
		0,
		"RICHEDIT50W",
		text.c_str(),
		WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL,
		x, y, width, height,
		hwnd,
		(HMENU)(intptr_t)id,
		m_wcex.hInstance,
		NULL
	);
	if (hRichEdit == nullptr) {
		MessageBox(nullptr, "リッチエディタの作成に失敗", "エラー", MB_OK | MB_ICONERROR);
		return nullptr;
	}
	SendMessage(hRichEdit, WM_SETFONT, (WPARAM)GetDefaultFont(), TRUE);
	m_childWindows[id] = hRichEdit;
	RegisterControlLayout(id, hRichEdit);
	ShowWindow(hRichEdit, SW_SHOW);
	UpdateWindow(hRichEdit);
	return hRichEdit;
}

void Window::SetRichEditPlaceholder(int id, const string& placeholder)
{
	m_placeholders[id] = placeholder;
	ShowRichEditPlaceholder(id);
}

void Window::ShowRichEditPlaceholder(int id)
{
	HWND hRichEdit = GetChildWindowHandle(id);
	auto it = m_placeholders.find(id);
	if (!hRichEdit || it == m_placeholders.end()) return;

	int len = GetWindowTextLengthA(hRichEdit);
	if (len > 0) return; // 既に何か入力されている場合は上書きしない

	SetWindowTextA(hRichEdit, it->second.c_str());

	CHARFORMATA cf = {};
	cf.cbSize = sizeof(cf);
	cf.dwMask = CFM_COLOR;
	cf.crTextColor = RGB(160, 160, 160); // プレースホルダーらしい薄いグレー
	SendMessage(hRichEdit, EM_SETSEL, 0, -1);
	SendMessage(hRichEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
	SendMessage(hRichEdit, EM_SETSEL, 0, 0);
}

void Window::HideRichEditPlaceholder(int id)
{
	HWND hRichEdit = GetChildWindowHandle(id);
	auto it = m_placeholders.find(id);
	if (!hRichEdit || it == m_placeholders.end()) return;

	char buf[1024] = {};
	GetWindowTextA(hRichEdit, buf, sizeof(buf));
	if (it->second != buf) return; // プレースホルダー表示中でなければ何もしない

	SetWindowTextA(hRichEdit, "");

	CHARFORMATA cf = {};
	cf.cbSize = sizeof(cf);
	cf.dwMask = CFM_COLOR;
	cf.crTextColor = RGB(0, 0, 0); // 通常の文字色に戻す(以降のタイプもこの色になる)
	SendMessage(hRichEdit, EM_SETSEL, 0, -1);
	SendMessage(hRichEdit, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
}

HWND Window::CreateListView(int id, int x, int y, int width, int height, DWORD style)
{
	INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_LISTVIEW_CLASSES };
	InitCommonControlsEx(&icex);

	HWND hListView = CreateWindowEx(
		0,
		WC_LISTVIEW, // "SysListView32"
		"",
		WS_CHILD | WS_VISIBLE | WS_BORDER | style,
		x, y, width, height,
		m_hWnd,
		(HMENU)(intptr_t)id,
		m_wcex.hInstance,
		NULL
	);
	if (!hListView) {
		MessageBox(m_hWnd, "リストビュー作成失敗", "エラー", MB_OK | MB_ICONERROR);
		return nullptr;
	}
	SendMessage(hListView, WM_SETFONT, (WPARAM)GetDefaultFont(), TRUE);

	m_childWindows[id] = hListView;
	RegisterControlLayout(id, hListView);
	ShowWindow(hListView, SW_SHOW);
	UpdateWindow(hListView);
	return hListView;
}

void Window::AddListViewColumn(int id, const string& text, int width)
{
	HWND hListView = GetChildWindowHandle(id);
	if (!hListView) {
		MessageBox(m_hWnd, "リストビューが見つかりません", "エラー", MB_OK | MB_ICONERROR);
		return;
	}
	// 現在のカラム数を取得して末尾に追加
	int colIndex = Header_GetItemCount(ListView_GetHeader(hListView));

	LVCOLUMNA lvc = { 0 };
	lvc.mask = LVCF_WIDTH | LVCF_TEXT;
	lvc.cx = width;
	lvc.pszText = (LPSTR)text.c_str();
	SendMessageA(hListView, LVM_INSERTCOLUMNA, colIndex, (LPARAM)&lvc); // ← colIndex を渡す
}

void Window::AddListViewItem(int id, int row, int col, const string& text)
{
	HWND hListView = GetChildWindowHandle(id);
	if (!hListView) {
		MessageBox(m_hWnd, "リストビューが見つかりません", "エラー", MB_OK | MB_ICONERROR);
		return;
	}

	int itemCount = (int)SendMessage(hListView, LVM_GETITEMCOUNT, 0, 0);

	if (col == 0) {
		if (row >= itemCount) {
			// 行が存在しない → 新規挿入
			LVITEMA lvi = { 0 };
			lvi.mask = LVIF_TEXT;
			lvi.iItem = row;
			lvi.pszText = (LPSTR)text.c_str();
			SendMessageA(hListView, LVM_INSERTITEMA, 0, (LPARAM)&lvi);
		}
		else {
			// 行がすでに存在する → テキスト上書き
			LVITEMA lvi = { 0 };
			lvi.iItem = row;
			lvi.iSubItem = 0;
			lvi.pszText = (LPSTR)text.c_str();
			SendMessageA(hListView, LVM_SETITEMTEXTA, row, (LPARAM)&lvi);
		}
	}
	else {
		LVITEMA lvi = { 0 };
		lvi.iItem = row;
		lvi.iSubItem = col;
		lvi.pszText = (LPSTR)text.c_str();
		SendMessageA(hListView, LVM_SETITEMTEXTA, row, (LPARAM)&lvi);
	}
}

HWND Window::CreateTreeView(int id, int x, int y, int width, int height)
{
	INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_TREEVIEW_CLASSES };
	InitCommonControlsEx(&icex);

	HWND hTreeView = CreateWindowEx(
		0,
		WC_TREEVIEW, // "SysTreeView32"
		"",
		WS_CHILD | WS_VISIBLE | WS_BORDER | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT,
		x, y, width, height,
		m_hWnd,
		(HMENU)(intptr_t)id,
		m_wcex.hInstance,
		NULL
	);
	if (!hTreeView) {
		MessageBox(m_hWnd, "ツリービュー作成失敗", "エラー", MB_OK | MB_ICONERROR);
		return nullptr;
	}
	SendMessage(hTreeView, WM_SETFONT, (WPARAM)GetDefaultFont(), TRUE);

	m_childWindows[id] = hTreeView;
	RegisterControlLayout(id, hTreeView);
	ShowWindow(hTreeView, SW_SHOW);
	UpdateWindow(hTreeView);
	return hTreeView;
}

HTREEITEM Window::AddTreeViewItem(int id, const string& text)
{
	HWND hTree = GetChildWindowHandle(id);

	TVINSERTSTRUCTA tvi = {};
	tvi.hParent = TVI_ROOT;
	tvi.hInsertAfter = TVI_LAST;
	tvi.item.mask = TVIF_TEXT;
	tvi.item.pszText = (LPSTR)text.c_str();
	HTREEITEM hParent = (HTREEITEM)SendMessageA(hTree, TVM_INSERTITEMA, 0, (LPARAM)&tvi);
	return hParent;
}

HTREEITEM Window::AddTreeViewItem(int id, HTREEITEM parent, const string& text)
{
	HWND hTree = GetChildWindowHandle(id);

	TVINSERTSTRUCTA tvi = {};
	tvi.hParent = parent;
	tvi.hInsertAfter = TVI_LAST;
	tvi.item.mask = TVIF_TEXT;
	tvi.item.pszText = (LPSTR)text.c_str();
	HTREEITEM hChild = (HTREEITEM)SendMessageA(hTree, TVM_INSERTITEMA, 0, (LPARAM)&tvi);
	return hChild;
}

HWND Window::CreateTabControl(int id, int x, int y, int width, int height)
{
	INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_TAB_CLASSES };
	InitCommonControlsEx(&icex);

	HWND hTab = CreateWindowEx(
		0,
		WC_TABCONTROL, // "SysTabControl32"
		"",
		WS_CHILD | WS_VISIBLE | WS_BORDER | TCS_FIXEDWIDTH | TCS_OWNERDRAWFIXED,
		x, y, width, height,
		m_hWnd,
		(HMENU)(intptr_t)id,
		m_wcex.hInstance,
		NULL
	);
	if (!hTab) {
		MessageBox(m_hWnd, "タブコントロール作成失敗", "エラー", MB_OK | MB_ICONERROR);
		return nullptr;
	}
	SendMessage(hTab, WM_SETFONT, (WPARAM)GetDefaultFont(), TRUE);

	m_childWindows[id] = hTab;
	RegisterControlLayout(id, hTab);
	ShowWindow(hTab, SW_SHOW);
	UpdateWindow(hTab);
	return hTab;
}

void Window::AddTabItem(int Id, const string& text)
{
	HWND hTab = GetChildWindowHandle(Id);
	if (!hTab) {
		MessageBox(m_hWnd, "タブコントロールが見つかりません", "エラー", MB_OK | MB_ICONERROR);
		return;
	}
	TCITEMA tci = {};
	tci.mask = TCIF_TEXT;
	tci.pszText = (LPSTR)text.c_str();
	int count = (int)SendMessage(hTab, TCM_GETITEMCOUNT, 0, 0);
	SendMessageA(hTab, TCM_INSERTITEMA, count, (LPARAM)&tci);
	AutoSizeTabItems(Id);
}

HWND Window::CreateTrackBar(int id, int x, int y, int width, int height, int min, int max, int pos, bool vertical)
{
	INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_BAR_CLASSES };
	InitCommonControlsEx(&icex);

	DWORD trackStyle = WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS;
	if (vertical) trackStyle |= TBS_VERT;

	HWND hTrack = CreateWindowEx(
		0,
		TRACKBAR_CLASS, // "msctls_trackbar32"
		"",
		trackStyle,
		x, y, width, height,
		m_hWnd,
		(HMENU)(intptr_t)id,
		m_wcex.hInstance,
		NULL
	);
	if (!hTrack) {
		MessageBox(m_hWnd, "トラックバー作成失敗", "エラー", MB_OK | MB_ICONERROR);
		return nullptr;
	}

	SendMessage(hTrack, TBM_SETRANGE, TRUE, MAKELPARAM(min, max));
	SendMessage(hTrack, TBM_SETPOS, TRUE, pos);

	m_childWindows[id] = hTrack;
	RegisterControlLayout(id, hTrack);
	ShowWindow(hTrack, SW_SHOW);
	UpdateWindow(hTrack);
	return hTrack;
}

HWND Window::CreateProgressBar(int id, int x, int y, int width, int height, int min, int max, int pos)
{
	INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_PROGRESS_CLASS };
	InitCommonControlsEx(&icex);

	HWND hProg = CreateWindowEx(
		0,
		PROGRESS_CLASS, // "msctls_progress32"
		"",
		WS_CHILD | WS_VISIBLE,
		x, y, width, height,
		m_hWnd,
		(HMENU)(intptr_t)id,
		m_wcex.hInstance,
		NULL
	);
	if (!hProg) {
		MessageBox(m_hWnd, "プログレスバー作成失敗", "エラー", MB_OK | MB_ICONERROR);
		return nullptr;
	}

	SendMessage(hProg, PBM_SETRANGE, 0, MAKELPARAM(min, max));
	SendMessage(hProg, PBM_SETPOS, pos, 0);

	m_childWindows[id] = hProg;
	RegisterControlLayout(id, hProg);
	ShowWindow(hProg, SW_SHOW);
	UpdateWindow(hProg);
	return hProg;
}

HWND Window::CreateText(int id, const string& text, int x, int y, int width, int height)
{
	HWND hStatic = CreateWindowEx(
		0,
		"STATIC",
		text.c_str(),
		WS_CHILD | WS_VISIBLE,
		x, y, width, height,
		m_hWnd,
		(HMENU)(intptr_t)id,
		m_wcex.hInstance,
		NULL
	);
	if (hStatic == nullptr) {
		MessageBox(nullptr, "テキストの作成に失敗", "エラー", MB_OK | MB_ICONERROR);
		return nullptr;
	}
	SendMessage(hStatic, WM_SETFONT, (WPARAM)GetDefaultFont(), TRUE);
	m_childWindows[id] = hStatic;
	RegisterControlLayout(id, hStatic);
	ShowWindow(hStatic, SW_SHOW);
	UpdateWindow(hStatic);
	return hStatic;
}

HWND Window::CreateSubWindow(int id, const string& title, int x, int y, int width, int height)
{
	HWND hSubWnd = CreateWindowEx(
		WS_EX_CLIENTEDGE,
		m_wcex.lpszClassName,
		title.c_str(),
		WS_CHILD | WS_VISIBLE,
		x, y, width, height,
		m_hWnd,
		(HMENU)(intptr_t)id,
		m_wcex.hInstance,
		NULL
	);
	if (hSubWnd == nullptr) {
		MessageBox(nullptr, "サブウインドウの作成に失敗", "エラー", MB_OK | MB_ICONERROR);
		return nullptr;
	}
	m_childWindows[id] = hSubWnd;
	RegisterControlLayout(id, hSubWnd);
	ShowWindow(hSubWnd, SW_SHOW);
	UpdateWindow(hSubWnd);
	return hSubWnd;
}

HWND Window::CreateSubSeparateWindow(int id, const string& title, int x, int y, int width, int height)
{
	HWND hSubWnd = CreateWindowEx(
		WS_EX_CLIENTEDGE,
		m_wcex.lpszClassName,
		title.c_str(),
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		x, y, width, height,
		nullptr,
		nullptr,
		m_wcex.hInstance,
		NULL
	);
	if (hSubWnd == nullptr) {
		MessageBox(nullptr, "サブウインドウの作成に失敗", "エラー", MB_OK | MB_ICONERROR);
		return nullptr;
	}
	m_childWindows[id] = hSubWnd;
	ShowWindow(hSubWnd, SW_SHOW);
	UpdateWindow(hSubWnd);
	return hSubWnd;
}

void Window::SetButtonClicked(int id)
{
	m_buttonClicked[id] = true;
}

bool Window::IsButtonClicked(int id)
{
	if (m_buttonClicked[id]) {
		m_buttonClicked[id] = false;
		return true;
	}
	return false;
}

string Window::GetEditText(int id)
{
	HWND hEdit = GetChildWindowHandle(id);
	if (!hEdit) return "";
	int len = GetWindowTextLengthA(hEdit);
	if (len <= 0) return "";
	std::string buf(len, '\0');
	GetWindowTextA(hEdit, &buf[0], len + 1);
	return buf;
}

int Window::GetListBoxCurSel(int id)
{
	HWND hList = GetChildWindowHandle(id);
	if (!hList) return -1;
	return (int)SendMessage(hList, LB_GETCURSEL, 0, 0);
}

int Window::GetComboBoxCurSel(int id)
{
	HWND hCombo = GetChildWindowHandle(id);
	if (!hCombo) return -1;
	return (int)SendMessage(hCombo, CB_GETCURSEL, 0, 0);
}

int Window::GetListViewCurSel(int id)
{
	HWND hListView = GetChildWindowHandle(id);
	if (!hListView) return -1;
	return (int)SendMessage(hListView, LVM_GETNEXTITEM, -1, LVNI_SELECTED);
}

string Window::GetListViewItemText(int id, int row, int col)
{
	HWND hListView = GetChildWindowHandle(id);
	if (!hListView) return "";

	char buffer[512] = {};
	LVITEMA lvi = {};
	lvi.iSubItem = col;
	lvi.cchTextMax = sizeof(buffer);
	lvi.pszText = buffer;
	lvi.mask = LVIF_TEXT;
	if (SendMessageA(hListView, LVM_GETITEMTEXTA, row, (LPARAM)&lvi) > 0) {
		return std::string(buffer);
	}
	return "";
}

HTREEITEM Window::GetTreeViewCurSel(int id)
{
	HWND hTreeView = GetChildWindowHandle(id);
	if (!hTreeView) return NULL;
	return (HTREEITEM)SendMessage(hTreeView, TVM_GETNEXTITEM, TVGN_CARET, 0);
}

string Window::GetTreeViewSelText(int id)
{
	HWND hTreeView = GetChildWindowHandle(id);
	if (!hTreeView) return "";
	HTREEITEM hItem = (HTREEITEM)SendMessage(hTreeView, TVM_GETNEXTITEM, TVGN_CARET, 0);
	if (!hItem) return "";
	char buffer[512] = {};
	TVITEMA tvi = {};
	tvi.hItem = hItem;
	tvi.cchTextMax = sizeof(buffer);
	tvi.pszText = buffer;
	tvi.mask = TVIF_TEXT;
	if (SendMessageA(hTreeView, TVM_GETITEMA, 0, (LPARAM)&tvi) > 0) {
		return std::string(buffer);
	}
	return "";
}

int Window::GetTabSel(int id)
{
	HWND hTab = GetChildWindowHandle(id);
	if (!hTab) return -1;
	return (int)SendMessage(hTab, TCM_GETCURSEL, 0, 0);
}

string Window::GetTabItemText(int id)
{
	HWND hTab = GetChildWindowHandle(id);
	if (!hTab) return "";

	int idx = GetTabSel(id);
	if (idx < 0) return "";

	char buffer[512] = {};
	TCITEMA tci = {};
	tci.mask = TCIF_TEXT;
	tci.pszText = buffer;
	tci.cchTextMax = sizeof(buffer);

	if (SendMessageA(hTab, TCM_GETITEMA, idx, (LPARAM)&tci)) {
		return std::string(buffer);
	}
	return "";
}

int Window::GetTrackBarPos(int id)
{
	HWND hTrack = GetChildWindowHandle(id);
	if (!hTrack) return 0;
	return (int)SendMessage(hTrack, TBM_GETPOS, 0, 0);
}

void Window::SetTrackBarPos(int id, int pos)
{
	HWND hTrack = GetChildWindowHandle(id);
	if (!hTrack) return;
	SendMessage(hTrack, TBM_SETPOS, TRUE, pos);
}

int Window::GetProgressBarPos(int id)
{
	HWND hProg = GetChildWindowHandle(id);
	if (!hProg) return 0;
	return (int)SendMessage(hProg, PBM_GETPOS, 0, 0);
}

void Window::SetProgressBarPos(int id, int pos)
{
	HWND hProg = GetChildWindowHandle(id);
	if (!hProg) return;
	SendMessage(hProg, PBM_SETPOS, pos, 0);
}

void Window::SetTabShowWindow(int tabID, int tabIndex, HWND hWnd, int childTabId)
{
	if (m_TabShowWindow[tabID].size() <= (size_t)tabIndex)
		m_TabShowWindow[tabID].resize(tabIndex + 1);

	m_TabShowWindow[tabID][tabIndex].push_back({ hWnd, childTabId });

	ShowWindow(hWnd, SW_HIDE);
}

void Window::SetBoxText(int id, const string& text)
{
	HWND hBox = GetChildWindowHandle(id);
	if (!hBox) {
		MessageBox(nullptr, "コントロールが見つかりません", "エラー", MB_OK | MB_ICONERROR);
		return;
	}
	SetWindowTextA(hBox, text.c_str());
}

void Window::RemoveListViewItem(int id, int index)
{
	HWND hListView = GetChildWindowHandle(id);
	if (!hListView) {
		MessageBox(m_hWnd, "リストビューが見つかりません", "エラー", MB_OK | MB_ICONERROR);
		return;
	}
	SendMessage(hListView, LVM_DELETEITEM, index, 0);
}

void Window::SetAlwaysOnTop(bool enable)
{
	HWND insertAfter = enable ? HWND_TOPMOST : HWND_NOTOPMOST;
	SetWindowPos(m_hWnd, insertAfter, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

std::string Window::GetListViewColumnText(int id, int col)
{
	int row = GetListViewCurSel(id);
	if (row < 0) return "";
	return GetListViewItemText(id, row, col);
}

void Window::ShowOrHideTabControls(int tabID, BOOL parentVisible) {
	int sel = GetTabSel(tabID);
	if (sel < 0) sel = 0;
	for (size_t i = 0; i < m_TabShowWindow[tabID].size(); ++i) {
		for (auto& ctrl : m_TabShowWindow[tabID][i]) {
			BOOL show = (i == (size_t)sel) && parentVisible;
			ShowWindow(ctrl.hwnd, show ? SW_SHOW : SW_HIDE);
			if (ctrl.childTabId != -1)
				ShowOrHideTabControls(ctrl.childTabId, show);
		}
	}
}

void Window::UpdateTabShowWindow(int tabID, bool parentVisible) {
	int currentSel = GetTabSel(tabID);
	if (currentSel < 0) currentSel = 0;

	auto it = m_TabShowWindow.find(tabID);
	if (it == m_TabShowWindow.end()) return;

	auto& tabPages = it->second;
	for (size_t i = 0; i < tabPages.size(); ++i) {
		for (auto& ctrl : tabPages[i]) {
			BOOL show = (i == (size_t)currentSel) && parentVisible;
			ShowWindow(ctrl.hwnd, show ? SW_SHOW : SW_HIDE);
			if (ctrl.childTabId != -1 && m_TabShowWindow.count(ctrl.childTabId)) {
				UpdateTabShowWindow(ctrl.childTabId, show);
			}
		}
	}
}

void Window::UpdateAllTabShowWindows() {
	// どの tabIndex にも登録されていないタブID = ルートタブ
	set<int> childTabIds;
	for (auto& tabPair : m_TabShowWindow) {
		for (auto& page : tabPair.second) {
			for (auto& ctrl : page) {
				if (ctrl.childTabId != -1)
					childTabIds.insert(ctrl.childTabId);
			}
		}
	}

	// Zオーダー調整
	for (auto& tabPair : m_TabShowWindow) {
		HWND hTab = GetChildWindowHandle(tabPair.first);
		if (hTab) SetWindowPos(hTab, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	}
	for (auto& tabPair : m_TabShowWindow) {
		for (auto& page : tabPair.second) {
			for (auto& ctrl : page) {
				SetWindowPos(ctrl.hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
			}
		}
	}

	// ルートタブのみ起点として再帰処理
	for (auto& tabPair : m_TabShowWindow) {
		if (childTabIds.count(tabPair.first) == 0) {
			UpdateTabShowWindow(tabPair.first, true);
		}
	}
}

void Window::SetWindowTitle(const std::string& title)
{
	m_windowTitle = title;
	if (m_hWnd == nullptr) return;
	SetWindowTextA(m_hWnd, title.c_str());
}

int Window::GetClientWidth() const
{
	RECT rect;
	GetClientRect(m_hWnd, &rect);
	return rect.right - rect.left;
}

int Window::GetClientHeight() const
{
	RECT rect;
	GetClientRect(m_hWnd, &rect);
	return rect.bottom - rect.top;
}

HWND Window::GetChildWindowHandle(int id)
{
	auto it = m_childWindows.find(id);
	if (it != m_childWindows.end()) {
		return it->second;
	}
	return nullptr;
}

void Window::AutoSizeTabItems(int id)
{
	HWND hTab = GetChildWindowHandle(id);
	if (!hTab) return;

	int itemCount = (int)SendMessage(hTab, TCM_GETITEMCOUNT, 0, 0);
	if (itemCount <= 0) return;

	RECT rc;
	GetClientRect(hTab, &rc);
	// 内部パディング分の余裕を見ておかないと、ほんの数pxのはみ出しでもスクロール矢印が
	// 出現し、選択タブ以外が隠れてしまう。安全マージンとして少し引いておく。
	UINT dpi = GetDpiForWindow(m_hWnd);
	int safetyMargin = MulDiv(6, (int)dpi, 96);
	int tabWidth = ((rc.right - rc.left) - safetyMargin) / itemCount;

	int tabHeight = MulDiv(24, (int)dpi, 96);

	TabCtrl_SetItemSize(hTab, tabWidth, tabHeight);
}