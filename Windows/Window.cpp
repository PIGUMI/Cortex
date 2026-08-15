#include "Window.h"
#include "DirectX12.h"
using namespace std;
#include <Windows.h>
#include <commctrl.h>
#include <set>

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

/* ウインドウプロシージャ */
LRESULT WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))return true;

	switch (message) {
	case WM_COMMAND:
	{
		int id = LOWORD(wParam);
		int code = HIWORD(wParam);
		if (code == BN_CLICKED) {
			Window::GetInstance()->SetButtonClicked(id);
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
	case WM_SIZE:
		if (wParam != SIZE_MINIMIZED)
		{
			UINT w = LOWORD(lParam);
			UINT h = HIWORD(lParam);
			DirectX::DirectX12::Get()->RequestResize(w, h);
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
		m_instance->m_wcex.lpszClassName = "Pixeon3";
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
	ShowWindow(m_hWnd, nCmdShow);
	UpdateWindow(m_hWnd);
	CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	return true;
}

HWND Window::CreateButton(int id, const string& text, int x, int y, int width, int height, HWND hwnd)
{
	if (hwnd == nullptr) hwnd = m_hWnd;
	HWND hButton = CreateWindowEx(
		0,
		"BUTTON",
		text.c_str(),
		WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
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
	m_childWindows[id] = hButton;
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
	m_childWindows[id] = hEdit;
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
	m_childWindows[id] = hTextBox;
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
	for (const auto& item : items) {
		SendMessage(hListBox, LB_ADDSTRING, 0, (LPARAM)item.c_str());
	}
	m_childWindows[id] = hListBox;
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
	for (const auto& item : items) {
		SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)item.c_str());
	}
	m_childWindows[id] = hComboBox;
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
	m_childWindows[id] = hRichEdit;
	ShowWindow(hRichEdit, SW_SHOW);
	UpdateWindow(hRichEdit);
	return hRichEdit;
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

	m_childWindows[id] = hListView;
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

	m_childWindows[id] = hTreeView;
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
		WS_CHILD | WS_VISIBLE | WS_BORDER,
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

	m_childWindows[id] = hTab;
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
	m_childWindows[id] = hStatic;
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