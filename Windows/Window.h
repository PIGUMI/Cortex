#pragma once
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/*
	クラス名：Window
	区　　分：シングルトン
	説　　明：ウインドウの管理を行うクラス
	作　　者：秋野翔太
	更　　新：　2026/02/26 作成開始 完成
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

class Window
{
public:
	static Window* GetInstance();
	static void DestroyInstance();
public:
	bool Initialize(HINSTANCE hInstance, int nCmdShow);

	HWND CreateButton(int id, const std::string& text, int x, int y, int width, int height, HWND hwnd = nullptr);
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
	int GetClientWidth() const;
	int GetClientHeight() const;
public:
	WNDCLASSEX m_wcex{};
	RECT rect;
	DWORD style = WS_OVERLAPPEDWINDOW;
	DWORD exStyle = WS_EX_APPWINDOW;
	std::map<int, std::vector<std::vector<TabPageControl>>> m_TabShowWindow;
private:
	int m_windowWidth = 720;
	int m_windowHeight = 480;
	HWND m_hWnd = nullptr;
	std::map<int, HWND> m_childWindows;
	std::map<int, bool> m_buttonClicked;
private:
	Window() = default;
	~Window() = default;
private:
	static Window* m_instance;
};