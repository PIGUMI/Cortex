#pragma once
#include <Windows.h>
#include <d3d12.h>

/*
* GUI
* Dear ImGui (docking ブランチ) のラッパ。シングルトン。
* Win32 + DirectX12 バックエンドの初期化・フレーム処理・後始末をまとめる。
*
* 呼び出し順:
*   DirectX12::Init() -> Descriptor::Init() -> GUI::Get()->Init(hwnd)
*   毎フレーム: GUI::BeginFrame() -> (ImGui の UI 構築) -> DirectX12::BeginDraw()
*               -> (シーン描画) -> GUI::EndFrame(cmdList) -> DirectX12::EndDraw()
*               -> GUI::RenderMultiViewport()
*   終了時: GUI::Del()  (DirectX12::Del() より前)
*/
class GUI
{
public:
	static GUI* Get();
	static void Del();

	/**
	 * @brief ImGui コンテキストと Win32 / DX12 バックエンドを初期化する
	 * @param hwnd メインウィンドウのハンドル
	 * @return 成功したら true
	 * @note DirectX12 と Descriptor の Init 後に呼ぶこと
	 */
	bool Init(HWND hwnd);

	/**
	 * @brief バックエンドと ImGui コンテキストを破棄する
	 */
	void Shutdown();

	/**
	 * @brief フレーム開始。この後に ImGui::Begin 等で UI を構築する
	 */
	void BeginFrame();

	/**
	 * @brief メインビューポートの ImGui 描画コマンドを積む
	 * @param commandList 記録中の Direct コマンドリスト (DirectX12::BeginDraw 済み)
	 * @note シーン描画をすべて積んだ後、DirectX12::EndDraw の前に呼ぶこと
	 */
	void EndFrame(ID3D12GraphicsCommandList* commandList);

	/**
	 * @brief ドッキング/マルチビューポートのサブウィンドウを描画する
	 * @note DirectX12::EndDraw の後に呼ぶこと。ViewportsEnable 無効時は何もしない
	 */
	void RenderMultiViewport();

	bool IsInitialized() const { return m_initialized; }

private:
	GUI() = default;
	~GUI() = default;

	bool m_initialized      = false;
	bool m_viewportsEnabled = false;

	static GUI* s_instance;
};
