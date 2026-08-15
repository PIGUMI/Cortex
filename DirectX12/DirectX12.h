/*
* 対応最低GPU
*  NVIDIA RTX30系
*  AMD RX 6000系
*  Intel Arc A系(Xe-HPG)
*			 B系
*/
#pragma once
#include <Windows.h>	// Windows APIの基本的な型や関数を定義するヘッダーファイル
#include <d3d12.h>		// Direct3D 12 APIのインターフェースを定義するヘッダーファイル
#include <dxgi1_6.h>	// DXGI 1.6 APIのインターフェースを定義するヘッダーファイル
#include <d3dx12.h>		// Direct3D 12のユーティリティ関数やクラスを定義するヘッダーファイル
#include <wrl/client.h> // Microsoft::WRL::ComPtrを使用するためのヘッダーファイル

#include <vector>
#include <queue>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;

namespace DirectX
{
	class DirectX12
	{
	public:
		enum class CmdListType
		{
			Direct,
			Compute,
			Copy
		};
	public:
		/**
		 * @brief DirectX12のインスタンスを取得する
		 * @return DirectX12のインスタンス
		 */
		static DirectX12* Get();
		/**
		 * @brief DirectX12のインスタンスを削除する
		 */
		static void Del();
	public:
		/**
		 * @brief DirectX12の初期化を行う
		 */
		void Init(HWND hwnd, UINT width, UINT height);

		/**
		 * @brief DirectX12の描画開始前の処理を行う
		 */
		void BeginDraw();

		/**
		 * @brief DirectX12の描画終了後の処理を行う
		 */
		void EndDraw();

		/**
		 * @brief 指定したコマンドリストをリセットして記録開始状態にする
		 * @param type 対象のコマンドリストの種類（省略時はCopy）
		 */
		void BeginCopy(CmdListType type = CmdListType::Copy);

		/**
		 * @brief 指定したコマンドリストをCloseし、実行・完了を待機する
		 * @param type 対象のコマンドリストの種類（省略時はCopy）
		 */
		void EndCopyAndWait(CmdListType type = CmdListType::Copy);

		/**
		 * @brief DirectX12のリサイズ要求を行う
		 * @param width 横幅
		 * @param height　縦幅
		 */
		void RequestResize(UINT width, UINT height);

		/**
		 * @brief DirectX12のリサイズ要求があった場合に、実際にリサイズ処理を行う
		 */
		void  ApplyResizeIfNeeded();

		/**
		 * @brief バックバッファを復元する
		 */
		void RestoreBackBuffers();

	public:// 解像度のゲッター
		/**
		 * @brief　横幅を取得する
		 * @return 横幅
		 */
		UINT GetWidth() const { return m_Width; }

		/**
		 * @brief 縦幅を取得する
		 * @return　縦幅
		 */
		UINT GetHeight() const { return m_Height; }

	public:// CommandQueueのゲッター
		/**
		 * @brief DirectX12のDirect Command Queueを取得する
		 * @return DirectX12のDirect Command Queue
		 */
		ID3D12CommandQueue* GetDirectCommandQueue() const { return m_DirectCommandQueue.Get(); }

		/**
		 * @brief DirectX12のCompute Command Queueを取得する
		 * @return DirectX12のCompute Command Queue
		 */
		ID3D12CommandQueue* GetComputeCommandQueue() const { return m_ComputeCommandQueue.Get(); }

		/**
		 * @brief DirectX12のCopy Command Queueを取得する
		 * @return DirectX12のCopy Command Queue
		 */
		ID3D12CommandQueue* GetCopyCommandQueue() const { return m_CopyCommandQueue.Get(); }
	public:// CommandListのゲッター
		/**
		 * @brief DirectX12のコマンドリストを取得する
		 * @param type コマンドリストの種類
		 * @return DirectX12のコマンドリスト
		 */
		ID3D12GraphicsCommandList* GetCommandList(CmdListType type) const { return m_commandLists[static_cast<int>(type)].Get(); }

		/**
		 * @brief DirectX12のDeviceを取得する
		 * @return DirectX12のDevice
		 */
		ID3D12Device* GetDevice() const { return m_device.Get(); }

	private:
		/**
		 * @brief DirectX12のリサイズ処理を行う
		 * @param width 横幅
		 * @param height 縦幅
		 */
		void OnResize(UINT width, UINT height);
	private:
		UINT m_pendingWidth, m_pendingHeight;	// 保留中の幅と高さ
		UINT m_Width, m_Height;					// 現在の幅と高さ
		bool m_isResizing = false;				// リサイズ中かどうかのフラグ
	private: //DXGI
		ComPtr<IDXGIFactory6> m_factory;	// DXGI Factory
		ComPtr<IDXGIAdapter4> m_adapter;	// GPU Adapter
	private: //D3D12
		ComPtr<ID3D12Device> m_device;		// D3D12 Device
	private: //CommandQueue
		ComPtr<ID3D12CommandQueue> m_DirectCommandQueue;	// Direct Command Queue
		ComPtr<ID3D12CommandQueue> m_ComputeCommandQueue;	// Compute Command Queue
		ComPtr<ID3D12CommandQueue> m_CopyCommandQueue;		// Copy Command Queue
	private: //SwapChain
		ComPtr<IDXGISwapChain3> m_swapChain;		// Swap Chain
	private: //DescriptorHeap
		ComPtr<ID3D12DescriptorHeap> m_rtvHeap;		// Render Target View Descriptor Heap
		ComPtr<ID3D12DescriptorHeap> m_dsvHeap;		// Depth Stencil View Descriptor Heap
		UINT m_rtvDescriptorSize;					// Render Target View Descriptor Size
		D3D12_CPU_DESCRIPTOR_HANDLE m_dsvHeapHandle;// Depth Stencil View Descriptor Handle
	private: //RenderTargetView
		ComPtr<ID3D12Resource> m_renderTargets[2];	// Render Target Views
		ComPtr<ID3D12Resource> m_depthStencil;		// Depth Stencil View
	private:// CommandAllocator & CommandList
		ComPtr<ID3D12CommandAllocator> m_commandAllocators[3];	// コマンドアロケーターの配列
		ComPtr<ID3D12GraphicsCommandList> m_commandLists[3];	// コマンドリストの配列
	private:// Fence
		ComPtr<ID3D12Fence> m_fences[3];						// フェンスの配列
		UINT m_fenceValues[3] = {};								// フェンスの値の配列
		HANDLE m_fenceEvents[3] = {};							// フェンスイベントの配列
	private:
		UINT m_bbIndex;	// バックバッファのインデックス

	private:
		DirectX12() = default;
		~DirectX12() = default;
	private:
		static DirectX12* instance;
	};

	/*
	* Desciptor
	* GPUリソースへの「アクセス方法を記述した情報」
	* リソースそのもではなく、シェーダーがどのように
	* リソースを解釈しアクセスするかを定義したもの
	*
	* DescriptorHeap
	* 複数のDescriptorをまとめて管理するためのオブジェクト
	* GPU/CPU両方からアクセス可能なメモリに配置される
	*/
	class Descriptor
	{
	public:
		static Descriptor* Get();
		static void Del();
	public:
		/**
		 * @brief DescriptorHeapの初期化を行う
		 * @param capacity DescriptorHeapの容量（デフォルトは4096）
		 * @return 初期化に成功した場合はtrue、失敗した場合はfalse
		 */
		bool Init(UINT capacity = 4096);

		/**
		 * @brief DescriptorHeapからdescriptorを一つ割り当てる
		 * @return 割り当てたdescriptorのインデックス。割り当てに失敗した場合はUINT_MAXを返す
		 */
		UINT Allocate();

		/**
		 * @brief DescriptorHeapにdescriptorを一つ解放する
		 * @param index 解放するdescriptorのインデックス
		 */
		void Free(UINT index);

		/**
		 * @brief DescriptorHeapのCPUハンドルを取得する
		 * @param index 取得するdescriptorのインデックス
		 * @return DescriptorHeapのCPUハンドル
		 */
		D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(UINT index) const;

		/**
		 * @brief DescriptorHeapのGPUハンドルを取得する
		 * @param index 取得するdescriptorのインデックス
		 * @return DescriptorHeapのGPUハンドル
		 */
		D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(UINT index) const;

		/**
		 * @brief DescriptorHeapを取得する
		 * @return DescriptorHeap
		 */
		ID3D12DescriptorHeap* GetHeap() const { return m_heap.Get(); }

		/**
		 * @brief Descriptorのサイズを取得する
		 * @return Descriptorのサイズ
		 */
		UINT GetDescriptorSize() const { return m_descriptorSize; }
	private:
		UINT m_capacity = 0;						// DescriptorHeapの容量
		ComPtr<ID3D12DescriptorHeap> m_heap;	// DescriptorHeap
		UINT m_descriptorSize;					// Descriptorのサイズ
		std::queue<UINT>m_freeQueue;			// 空きDescriptorのキュー
		std::vector<bool> m_allocated;			// Descriptorの使用状況を管理するベクター
	private:
		Descriptor() = default;
		~Descriptor() = default;
	private:
		static Descriptor* instance;
	};

	Microsoft::WRL::ComPtr<ID3D12Resource> CreateUploadBuffer(size_t size, const void* initData);
}
