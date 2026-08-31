#pragma once
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/*
	クラス名：Window
	区　　分：シングルトン
	説　　明：ウインドウの管理を行うクラス
	作　　者：Garu
	更　　新：2026/08/16
*/
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/*
*  Create系関数：id ウインドウ識別ID (同じIDのウインドウは存在しないことが前提)
*				 text ウインドウに表示するテキスト (テキストを表示しないウインドウの場合は空文字列を渡す)
* 				 x ウインドウの左上のX座標
* 				 y ウインドウの左上のY座標
* 				 width ウインドウの幅
* 				 height ウインドウの高さ
*/

#include <Windows.h>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <Richedit.h>
#include <RichOle.h>
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

// NOTE: using namespace std をヘッダから削除
// (std::byte と Windows SDK の byte の衝突を防ぐため)

struct TabPageControl {
	HWND hwnd;
	int childTabId;
};

// SetControlColorで設定した文字色・背景色を保持する。
// backgroundBrushはWM_CTLCOLOR*が返すブラシとして使い回すためのキャッシュ。
struct ControlColor {
	COLORREF textColor = RGB(0, 0, 0);
	COLORREF backgroundColor = RGB(255, 255, 255);
	HBRUSH backgroundBrush = nullptr;
};

// コントロール作成時点の「親のクライアント領域に対する割合」を保持し、
// リサイズのたびにその割合を保ったまま再配置するための情報。
struct ControlLayout {
	int id;
	double fracX;
	double fracY;
	double fracWidth;
	double fracHeight;
};

// 自作タイトルバー(帯)に置くボタンのID。
// アプリ側が使うCreate系のidと衝突しないよう大きな番号を予約する。
enum TitleBarButtonId : int {
	ID_TITLEBAR_FILE = 40001,
	ID_TITLEBAR_EDIT = 40002,
	ID_TITLEBAR_MIN = 40003,
	ID_TITLEBAR_MAXRESTORE = 40004,
	ID_TITLEBAR_CLOSE = 40005,
};

class Window
{
public:
	static Window* GetInstance();
	static void DestroyInstance();
public:
	bool Initialize(HINSTANCE hInstance, int nCmdShow);

	HWND CreateButton(int id, const std::string& text, int x, int y, int width, int height, HWND hwnd = nullptr,bool flat = false);
	HWND CreateEdit(int id, const std::string& text, int x, int y, int width, int height);
	HWND CreateTextBox(int id, const std::string& text, int x, int y, int width, int height, HWND hwnd = nullptr);
	void AddTextBoxText(int id, const std::string& text);
	HWND CreateListBox(int id, const std::vector<std::string>& items, int x, int y, int width, int height);
	void AddListBoxItem(int id, const std::string& item);
	void RemoveListBoxItem(int id, int index);
	HWND CreateComboBox(int id, const std::vector<std::string>& items, int x, int y, int width, int height);
	void AddComboBoxItem(int id, const std::string& item);
	void RemoveComboBoxItem(int id, int index);
	HWND CreateScrollBar(int id, int x, int y, int width, int height, bool isVertical);
	HWND CreateRichEdit(int id, const std::string& text, int x, int y, int width, int height, HWND hwnd = nullptr);
	void SetControlColor(int id, COLORREF textColor, COLORREF backgroundColor);
	void SetRichEditPlaceholder(int id, const std::string& placeholder);
	void ShowRichEditPlaceholder(int id);
	void HideRichEditPlaceholder(int id);
	HWND CreateListView(int id, int x, int y, int width, int height, DWORD style = LVS_REPORT);
	void AddListViewColumn(int id, const std::string& text, int width);
	void AddListViewItem(int id, int row, int col, const std::string& text);
	HWND CreateTreeView(int id, int x, int y, int width, int height);
	HTREEITEM AddTreeViewItem(int id, const std::string& text);
	HTREEITEM AddTreeViewItem(int id, HTREEITEM parent, const std::string& text);
	HWND CreateTabControl(int id, int x, int y, int width, int height);
	void AddTabItem(int Id, const std::string& text);
	HWND CreateTrackBar(int id, int x, int y, int width, int height, int min, int max, int pos, bool vertical = false);
	HWND CreateProgressBar(int id, int x, int y, int width, int height, int min, int max, int pos);
	HWND CreateText(int id, const std::string& text, int x, int y, int width, int height);
	HWND CreateSubWindow(int id, const std::string& title, int x, int y, int width, int height);
	HWND CreateSubSeparateWindow(int id, const std::string& title, int x, int y, int width, int height);
public:
	void SetButtonClicked(int id);
	bool IsButtonClicked(int id);
	std::string GetEditText(int id);
	int GetListBoxCurSel(int id);
	int GetComboBoxCurSel(int id);
	int GetListViewCurSel(int id);
	std::string GetListViewItemText(int id, int row, int col);
	HTREEITEM GetTreeViewCurSel(int id);
	std::string GetTreeViewSelText(int id);
	int GetTabSel(int id);
	std::string GetTabItemText(int id);
	int GetTrackBarPos(int id);
	void SetTrackBarPos(int id, int pos);
	int GetProgressBarPos(int id);
	void SetProgressBarPos(int id, int pos);
	void SetTabShowWindow(int tabID, int tabIndex, HWND hWnd, int childTabId = -1);
	void SetBoxText(int id, const std::string& text);
	void RemoveListViewItem(int id, int index);
	void SetAlwaysOnTop(bool enable);
	std::string GetListViewColumnText(int id, int col);
public:
	void SetWindowWidth(int width) { m_windowWidth = width; }
	void SetWindowHeight(int height) { m_windowHeight = height; }
	int GetWindowWidth() const { return m_windowWidth; }
	int GetWindowHeight() const { return m_windowHeight; }
	HWND GetMainWindowHandle() const { return m_hWnd; }
	HWND GetChildWindowHandle(int id);
	void UpdateAllTabShowWindows();
	void UpdateTabShowWindow(int tabID, bool parentVisible = true);
	void ShowOrHideTabControls(int tabID, BOOL parentVisible);
	const std::map<int, HWND>& GetChildWindows() const { return m_childWindows; }
	std::string GetWindowTitle() const { return m_windowTitle; }
	void SetWindowTitle(const std::string& title);
	int GetClientWidth() const;
	int GetClientHeight() const;
	int GetTitleBarHeight() const { return m_titleBarHeight; }
	void LayoutTitleBarButtons();
	void ShowTitleBarMenu(int buttonId);
	void DrawFlatButton(LPDRAWITEMSTRUCT drawItem);
	void DrawTabItem(LPDRAWITEMSTRUCT drawItem);
	void SetResizeCallback(std::function<void(int, int)> callback) { m_onResize = callback; }
	void NotifyResize(int width, int height) { if (m_onResize) m_onResize(width, height); }
	void ApplyProportionalLayout();
	void AutoSizeTabItems(int id);
	HBRUSH HandleCtlColor(int id, HDC hdc);
public:
	WNDCLASSEX m_wcex{};
	RECT rect;
	DWORD style = WS_OVERLAPPEDWINDOW;
	DWORD exStyle = WS_EX_APPWINDOW;
	std::map<int, std::vector<std::vector<TabPageControl>>> m_TabShowWindow;
private:
	int m_windowWidth = 720;
	int m_windowHeight = 480;
	std::string m_windowTitle = "Window";
	HWND m_hWnd = nullptr;
	std::map<int, HWND> m_childWindows;
	std::map<int, bool> m_buttonClicked;
private:
	Window() = default;
	~Window() = default;
private:
	void ApplyModernWindowStyle(HWND hwnd);
	HFONT GetDefaultFont();
	void CreateTitleBar();
	void RegisterControlLayout(int id, HWND hControl);
private:
	static Window* m_instance;
	HFONT m_defaultFont = nullptr;
	int m_titleBarHeight = 0;
	std::function<void(int, int)> m_onResize;
	std::vector<ControlLayout> m_controlLayouts;
	std::map<int, std::string> m_placeholders;
	std::map<int, ControlColor> m_controlColors;
};