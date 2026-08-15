#include "DirectX12.h"

// ============================================================================================================================= DirectX12

DirectX::DirectX12* DirectX::DirectX12::instance = nullptr;

DirectX::DirectX12* DirectX::DirectX12::Get()
{
	if (instance == nullptr)
	{
		instance = new DirectX12();
	}
	return instance;
}

void DirectX::DirectX12::Del()
{
	if (instance != nullptr)
	{
		delete instance;
		instance = nullptr;
	}
}

void DirectX::DirectX12::Init(HWND hwnd, UINT width, UINT height)
{
	/*
	* IDXGIFactory6とは
	* DXGIというDx12とは別のAPIの一部
	* DXGIはGPUやディスプレイ出力などのハードウェアを抽象化する低レイヤー
	* Dx12はレンダリングを命令を担当、DXGIは「周辺設備」を担当する
	*/

	// ============================================================================================================================ Factory & Device

	/*
	* Factoryの役割
	* GPU(Adapter)の列挙と選択
	*	PCに刺さっているGPUを全部リストアップして、どれを使用するか選択
	* SwapChainの作成
	*	描画結果を画面に表示するためのバックバッファを管理するSwapChainはDXGI側が作る。
	*	DX12のCommandQueueと組み合わせて使う
	*/
	{
		UINT factoryFlags = 0;
		CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&m_factory)); // DXGI Factoryの作成

		m_factory->EnumAdapterByGpuPreference(
			0,										// 最も高性能なGPUを選択
			DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,	// GPUの選択基準
			/*
			* HIGH_PERFORMANCE	: xGPU→dGPU→iGPU→WARP
			* MINIMUM_POWER		: iGPU→dGPU→xGPU
			* UNSPECIFIED		: OSまかせ
			*/
			IID_PPV_ARGS(&m_adapter)				// GPU Adapterの取得
		);

		/*
		* デバイスの作成
		* Dx12の機能を使用するためには、選択したGPUに対してD3D12Deviceを作成する必要がある
		*/
		HRESULT hr = D3D12CreateDevice(
			m_adapter.Get(),
			D3D_FEATURE_LEVEL_12_2, // レベルの設定
			IID_PPV_ARGS(&m_device)
		);
		/*
		* Dx12の推奨レベル別の機能
		* 11_0 : 幅広いGPUでサポートされている基本的な機能
		* 12_0 : 現代のGPUを前提にした機能
		* 12_2 : レイトレーシング・Mesh Shaderなどの最新機能をサポートするGPU向け
		*/

		if (FAILED(hr))
		{
			MessageBox(nullptr, "DirectX12のドライバー作成に失敗しました\nGPUがLEVEL_12_2に対応しているか確認してください", "DirectX12の初期化エラー", MB_OK);
			return;
		}
	}

	// ============================================================================================================================ Command系の作成

	/*
	* CommandQueueとは
	* CPUが記録した命令をGPUに送るためのもの
	* コマンドキューの役割はGPUへ送るだけで、コマンドの中身を解釈したりシェーダーを実行したりするのはGPU側の仕事
	* CommandQueueは3種類のキュー
	* Direct Queue : 描画・コンピュート・コピー　すべて実行できる
	* Compute Queue: コンピュートシェーダーのみ
	* Copy Queue   : リソースのコピーのみ（最も軽量）
	*/
	{
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;					// DirectQueueを作成
		queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;			// 優先度は通常
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;					// フラグはなし
		queueDesc.NodeMask = 0;												// ノードマスクは0（シングルGPUの場合は常に0）
		m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_DirectCommandQueue)); // CommandQueueの作成

		D3D12_COMMAND_QUEUE_DESC computeQueueDesc = {};
		computeQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;			// ComputeQueueを作成
		computeQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;	// 優先度は通常
		computeQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;				// フラグはなし
		computeQueueDesc.NodeMask = 0;										// ノードマスクは0（シングルGPUの場合は常に0）
		m_device->CreateCommandQueue(&computeQueueDesc, IID_PPV_ARGS(&m_ComputeCommandQueue)); // CommandQueueの作成

		D3D12_COMMAND_QUEUE_DESC copyQueueDesc = {};
		copyQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;					// CopyQueueを作成
		copyQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;		// 優先度は通常
		copyQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;				// フラグはなし
		copyQueueDesc.NodeMask = 0;											// ノードマスクは0（シングルGPUの場合は常に0）
		m_device->CreateCommandQueue(&copyQueueDesc, IID_PPV_ARGS(&m_CopyCommandQueue)); // CommandQueueの作成
	}

	/*
	* コマンドアロケータ＆コマンドリスト
	* コマンドアロケーターは、コマンドリストのメモリを管理するオブジェクト
	*/
	{
		// コマンドアロケーターの作成・取得
		m_device->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,				// Direct Command Allocatorを作成
			IID_PPV_ARGS(&m_commandAllocators[static_cast<int>(CmdListType::Direct)])		// コマンドアロケーターの取得
		);

		m_device->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_COMPUTE,			// Compute Command Allocatorを作成
			IID_PPV_ARGS(&m_commandAllocators[static_cast<int>(CmdListType::Compute)])	// コマンドアロケーターの取得
		);

		m_device->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_COPY,				// Copy Command Allocatorを作成
			IID_PPV_ARGS(&m_commandAllocators[static_cast<int>(CmdListType::Copy)])		// コマンドアロケーターの取得
		);

		// コマンドリストの作成・取得
		m_device->CreateCommandList(
			0,
			D3D12_COMMAND_LIST_TYPE_DIRECT,				// Direct Command Listを作成
			m_commandAllocators[static_cast<int>(CmdListType::Direct)].Get(),				// コマンドアロケーター
			nullptr,
			IID_PPV_ARGS(&m_commandLists[static_cast<int>(CmdListType::Direct)])			// コマンドリストの取得
		);

		m_device->CreateCommandList(
			0,
			D3D12_COMMAND_LIST_TYPE_COMPUTE,			// Compute Command Listを作成
			m_commandAllocators[static_cast<int>(CmdListType::Compute)].Get(),			// コマンドアロケーター
			nullptr,
			IID_PPV_ARGS(&m_commandLists[static_cast<int>(CmdListType::Compute)])			// コマンドリストの取得
		);

		m_device->CreateCommandList(
			0,
			D3D12_COMMAND_LIST_TYPE_COPY,				// Copy Command Listを作成
			m_commandAllocators[static_cast<int>(CmdListType::Copy)].Get(),				// コマンドアロケーター
			nullptr,
			IID_PPV_ARGS(&m_commandLists[static_cast<int>(CmdListType::Copy)])			// コマンドリストの取得
		);

		for (int i = 0; i < 3; i++)m_commandLists[i]->Close(); // コマンドリストは最初にCloseしておく（Reset()するため）
	}

	// ============================================================================================================================ 描画に必要なオブジェクトの作成

	/*
	* SwapChainの作成
	* SwapChainは描画結果を画面に表示するためのオブジェクト
	* バックバッファ
	*	GPUが描画する先のVRAM上のテクスチャ　これが完成してからモニターに表示
	* Present()
	*	描画が終わったあとPresent()を呼ぶとバックバッファとフロントバッファを入れ替える
	*/
	{
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
		/* 解像度 */
		swapChainDesc.Width = width;
		swapChainDesc.Height = height;
		/* バックバッファのフォーマット */
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.SampleDesc.Count = 1;	// マルチサンプリングなし
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // 描画ターゲットとして使用
		swapChainDesc.BufferCount = 2;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // フリップモデル
		ComPtr<IDXGISwapChain1> swapChain1;
		/* スワップチェーンの作成 */
		m_factory->CreateSwapChainForHwnd(
			m_DirectCommandQueue.Get(), // コマンドキュー
			hwnd,						// ウインドウハンドル
			&swapChainDesc,				// スワップチェーンの設定
			nullptr,					// フルスクリーン設定（今回はウインドウモードなのでnullptr）
			nullptr,
			&swapChain1					// 作成したスワップチェーンを受け取る
		);

		/*
		* SwapChain1とSwapChain3の違い
		* GetCurrentBackBufferIndex()がSwapChain1にはないため,現在のバックバッファのインデックスを取得できない
		*/
		swapChain1.As(&m_swapChain); //IDXGISwapChain1をIDXGISwapChain3に変換して保存

		m_factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER); // Alt+Enterでフルスクリーン切り替えを無効化
	}

	/*
	* RenderTargetViewの作成
	* GPUが描画する先のキャンパス
	* RenderTargetは「Resource(実体)」と「RTV(ビュー)」の2つがセット
	* Resource
	*	VRAMに確保された実際のピクセルデータ、SwapChainのBackBufferはGetBuffer()で取得できるので
	*	自分で確保する確保する必要はない
	* RTV
	*	「このResourceを描画先として使う」という記述子です。GPUはRTVを見て
	*	「どのメモリに書けばいいか」判断する
	*/
	{
		// RTVを作るためのヒープの設定
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = 2;						// バックバッファの数と同じにする
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;	// RTV用のヒープ
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;// フラグはなし

		m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)); // RTVヒープの作成

		// ディスクリプタ、1個のサイズはGPUによって異なるため、取得しておく
		m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		// RTVヒープの先頭ハンドルを取得
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

		/*
		* SwapChainが持つBackBuffer(VRAM上の場所）
		*	↓	GetBuffer()でResourceとして取り出す
		*	↓	CreateRenderTargetView()でGPU用形式に変換
		* DescriptorHeapのスロットに焼き込む
		*/
		for (int i = 0; i < 2; i++)
		{
			// スワップチェーンからバックバッファを取得
			m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i]));

			// RTVをヒープに書き込む
			m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHeapHandle);

			// 次のスロットへ進む
			rtvHeapHandle.Offset(m_rtvDescriptorSize);
		}
	}

	/*
	* Depth Stencil Viewとは
	*	GPUが描画するときに奥行き情報を管理するバッファ
	* RTVはSwapChainからGetBuffer()でResourceを取得できるが
	* DSVは自分でResourceを作成する必要がある
	*/
	{
		D3D12_DESCRIPTOR_HEAP_DESC DepthStencilViewHeapDesc = {};
		DepthStencilViewHeapDesc.NumDescriptors = 1;
		DepthStencilViewHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		DepthStencilViewHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		m_device->CreateDescriptorHeap(&DepthStencilViewHeapDesc, IID_PPV_ARGS(&m_dsvHeap));

		CD3DX12_HEAP_PROPERTIES heapProp(D3D12_HEAP_TYPE_DEFAULT);
		/*
		* このResourceをどのメモリに置くかの設定
		* DEFAULT : GPU読み書き〇　CPU読み書き×
		*/

		CD3DX12_RESOURCE_DESC depthDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			DXGI_FORMAT_D32_FLOAT,
			width, height,
			1,
			0,
			1,
			0,
			D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
		/*
		* 作成するResourceの設定
		* フォーマット
		* 解像度
		* 配列サイズ（一枚）
		* ミップレベル（０＝自動）
		* サンプル数（MSAAなし）
		* サンプル品質
		* 深度バッファとして使う
		*/

		D3D12_CLEAR_VALUE clearValue = {};
		clearValue.Format = DXGI_FORMAT_D32_FLOAT;
		clearValue.DepthStencil.Depth = 1.0f;
		clearValue.DepthStencil.Stencil = 0;
		/*
		* このResourceをクリアするときの値を指定
		* FormatはResourceDescと同じFormatを指定する必要がある
		*/

		m_device->CreateCommittedResource(
			&heapProp, D3D12_HEAP_FLAG_NONE,
			&depthDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&clearValue, IID_PPV_ARGS(&m_depthStencil)
		);
		/*
		* GPUのメモリを確保してResourceを作成する
		* メモリ設定
		* リソースフォーマット
		* 初期化値
		* がいる
		*/

		m_dsvHeapHandle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

		m_device->CreateDepthStencilView(m_depthStencil.Get(), &dsvDesc, m_dsvHeapHandle);
		/*
		* 深度バッファのResourceをGPU用形式に変換してHeapに書き込む
		*/
	}

	// ============================================================================================================================　CPU & GPUの同期のためのオブジェクトの作成

	/*
	* フェンスの作成
	* フェンスは,GPUの処理が完了したかどうかをCPU側で確認するためのオブジェクト
	*/
	{
		for (int i = 0; i < 3; i++)
		{
			m_fenceValues[i] = 0; // フェンスの値を初期化

			m_device->CreateFence(
				m_fenceValues[i],
				D3D12_FENCE_FLAG_NONE,
				IID_PPV_ARGS(&m_fences[i])
			);

			m_fenceEvents[i] = CreateEvent(
				nullptr,	// セキュリティ属性
				FALSE,		// 手動リセット不要
				FALSE,		// 初期状態はシグナルなし
				nullptr 	// イベント名
			);

			if (m_fenceEvents[i] == nullptr)
			{
				MessageBox(nullptr, "フェンスイベントの作成に失敗しました", "DirectX12の初期化エラー", MB_OK);
				return;
			}
		}
	}

	m_pendingHeight = height;
	m_pendingWidth = width;
	m_Width = width;
	m_Height = height;

	return;
}

void DirectX::DirectX12::BeginDraw()
{
	//どのバッファに描画するかの番号を取得
	m_bbIndex = m_swapChain->GetCurrentBackBufferIndex();
	// GPUコマンドを記録するためのコマンドリストをリセット
	m_commandAllocators[static_cast<int>(CmdListType::Direct)]->Reset();
	m_commandLists[static_cast<int>(CmdListType::Direct)]->Reset(m_commandAllocators[static_cast<int>(CmdListType::Direct)].Get(), nullptr); // コマンドリストをリセット
	// コマンドリストにDescriptorHeapをセットするために、DescriptorクラスからDescriptorHeapを取得
	auto* heap = DirectX::Descriptor::Get()->GetHeap();
	m_commandLists[static_cast<int>(CmdListType::Direct)]->SetDescriptorHeaps(1, &heap); // コマンドリストにDescriptorHeapをセット

	D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		m_renderTargets[m_bbIndex].Get(),
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET
	);
	m_commandLists[static_cast<int>(CmdListType::Direct)]->ResourceBarrier(1, &barrier); // バックバッファを描画可能状態に遷移

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
		m_rtvHeap->GetCPUDescriptorHandleForHeapStart(),
		m_bbIndex,
		m_rtvDescriptorSize
	);

	m_commandLists[static_cast<int>(CmdListType::Direct)]->OMSetRenderTargets(1, &rtvHandle, FALSE, &m_dsvHeapHandle); // レンダーターゲットとデプスステンシルビューをセット

	const float clearColor[] = { 0.0f,1.0f,1.0f,1.0f };
	m_commandLists[static_cast<int>(CmdListType::Direct)]->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr); // レンダーターゲットをクリア

	m_commandLists[static_cast<int>(CmdListType::Direct)]->ClearDepthStencilView(m_dsvHeapHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr); // デプスステンシルビューをクリア

	D3D12_VIEWPORT viewport = {};
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = (float)m_Width;
	viewport.Height = (float)m_Height;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	D3D12_RECT scissorRect = {};
	scissorRect.left = 0;
	scissorRect.top = 0;
	scissorRect.right = (LONG)m_Width;
	scissorRect.bottom = (LONG)m_Height;

	m_commandLists[static_cast<int>(CmdListType::Direct)]->RSSetViewports(1, &viewport); // ビューポートをセット
	m_commandLists[static_cast<int>(CmdListType::Direct)]->RSSetScissorRects(1, &scissorRect); // シザー矩形をセット
}

void DirectX::DirectX12::EndDraw()
{
	D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		m_renderTargets[m_bbIndex].Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PRESENT
	);
	m_commandLists[static_cast<int>(CmdListType::Direct)]->ResourceBarrier(1, &barrier); // バックバッファをプレゼント状態に遷移

	m_commandLists[static_cast<int>(CmdListType::Direct)]->Close(); // コマンドリストをクローズ

	ID3D12CommandList* cmdLists[] = { m_commandLists[static_cast<int>(CmdListType::Direct)].Get() };
	m_DirectCommandQueue->ExecuteCommandLists(1, cmdLists); // コマンドリストをGPUに送る

	m_swapChain->Present(1, 0); // バックバッファとフロントバッファを入れ替える

	m_fenceValues[static_cast<int>(CmdListType::Direct)]++; // フェンスの値をインクリメント
	m_DirectCommandQueue->Signal(m_fences[static_cast<int>(CmdListType::Direct)].Get(), m_fenceValues[static_cast<int>(CmdListType::Direct)]); // GPUにフェンスの値をシグナルする

	if (m_fences[static_cast<int>(CmdListType::Direct)]->GetCompletedValue() < m_fenceValues[static_cast<int>(CmdListType::Direct)])
	{
		m_fences[static_cast<int>(CmdListType::Direct)]->SetEventOnCompletion(m_fenceValues[static_cast<int>(CmdListType::Direct)], m_fenceEvents[static_cast<int>(CmdListType::Direct)]);
		WaitForSingleObject(m_fenceEvents[static_cast<int>(CmdListType::Direct)], INFINITE);
	}
}

void DirectX::DirectX12::BeginCopy(CmdListType type)
{
	auto idx = static_cast<int>(type);
	m_commandAllocators[idx]->Reset();
	m_commandLists[idx]->Reset(m_commandAllocators[idx].Get(), nullptr);
}

void DirectX::DirectX12::EndCopyAndWait(CmdListType type)
{
	auto idx = static_cast<int>(type);
	m_commandLists[idx]->Close();

	ID3D12CommandList* list[] = { m_commandLists[idx].Get() };

	// 種類ごとに実行キューを切り替える
	// （Copy Queueは COMMON / COPY_DEST / COPY_SOURCE 以外の状態へ遷移するバリアを張れないため、
	//   PIXEL_SHADER_RESOURCEなどへの遷移はDirect QueueやCompute Queueで実行する必要がある）
	ID3D12CommandQueue* queue = nullptr;
	switch (type)
	{
	case CmdListType::Direct:  queue = m_DirectCommandQueue.Get();  break;
	case CmdListType::Compute: queue = m_ComputeCommandQueue.Get(); break;
	case CmdListType::Copy:    queue = m_CopyCommandQueue.Get();    break;
	}
	queue->ExecuteCommandLists(1, list);

	m_fenceValues[idx]++;
	queue->Signal(m_fences[idx].Get(), m_fenceValues[idx]);

	if (m_fences[idx]->GetCompletedValue() < m_fenceValues[idx])
	{
		m_fences[idx]->SetEventOnCompletion(m_fenceValues[idx], m_fenceEvents[idx]);
		WaitForSingleObject(m_fenceEvents[idx], INFINITE);
	}
}

void DirectX::DirectX12::RequestResize(UINT width, UINT height)
{
	if (width == 0 || height == 0)return;
	if (width == m_Width && height == m_Height)return;
	m_pendingHeight = height;
	m_pendingWidth = width;
	m_isResizing = true;
}

void DirectX::DirectX12::ApplyResizeIfNeeded()
{
	if (!m_isResizing)return;
	m_isResizing = false;
	OnResize(m_pendingWidth, m_pendingHeight);
}

void DirectX::DirectX12::RestoreBackBuffers()
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
		m_rtvHeap->GetCPUDescriptorHandleForHeapStart(),
		m_bbIndex,
		m_rtvDescriptorSize
	);

	m_commandLists[static_cast<int>(CmdListType::Direct)]->OMSetRenderTargets(1, &rtvHandle, FALSE, &m_dsvHeapHandle);

	D3D12_VIEWPORT vp = { 0,0,(float)m_Width,(float)m_Height,0.0f,1.0f };
	D3D12_RECT sr = { 0,0,(LONG)m_Width,(LONG)m_Height };
	m_commandLists[static_cast<int>(CmdListType::Direct)]->RSSetViewports(1, &vp);
	m_commandLists[static_cast<int>(CmdListType::Direct)]->RSSetScissorRects(1, &sr);
}

void DirectX::DirectX12::OnResize(UINT width, UINT height)
{
	if (!m_swapChain) return;

	// GPU完了を待つ
	m_fenceValues[static_cast<int>(CmdListType::Direct)]++;
	m_DirectCommandQueue->Signal(m_fences[static_cast<int>(CmdListType::Direct)].Get(), m_fenceValues[static_cast<int>(CmdListType::Direct)]);

	// GPUがまだ処理を完了していない場合は、フェンスイベントをセットして待機
	if (m_fences[static_cast<int>(CmdListType::Direct)]->GetCompletedValue() < m_fenceValues[static_cast<int>(CmdListType::Direct)])
	{
		m_fences[static_cast<int>(CmdListType::Direct)]->SetEventOnCompletion(m_fenceValues[static_cast<int>(CmdListType::Direct)], m_fenceEvents[static_cast<int>(CmdListType::Direct)]);
		// フェンスイベントがシグナルされるまで待機
		WaitForSingleObject(m_fenceEvents[static_cast<int>(CmdListType::Direct)], INFINITE);
	}

	// バックバッファを解放
	for (int i = 0; i < 2; i++)m_renderTargets[i].Reset();

	// DepthStencilを解放
	m_depthStencil.Reset();

	// SwapChainのリサイズ
	m_swapChain->ResizeBuffers(
		2,
		width, height,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		0
	);

	m_Width = width;
	m_Height = height;

	// RTVの再作成
	CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

	for (int i = 0; i < 2; i++)
	{
		m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i]));

		m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, handle);

		handle.Offset(m_rtvDescriptorSize);
	}

	// DepthStencilの再作成
	CD3DX12_HEAP_PROPERTIES heapProp(D3D12_HEAP_TYPE_DEFAULT);
	CD3DX12_RESOURCE_DESC depthDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		DXGI_FORMAT_D32_FLOAT,
		width, height,
		1,
		0,
		1,
		0,
		D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
	);

	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = DXGI_FORMAT_D32_FLOAT;
	clearValue.DepthStencil.Depth = 1.0f;
	clearValue.DepthStencil.Stencil = 0;

	m_device->CreateCommittedResource(
		&heapProp, D3D12_HEAP_FLAG_NONE,
		&depthDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&clearValue, IID_PPV_ARGS(&m_depthStencil)
	);

	m_dsvHeapHandle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	m_device->CreateDepthStencilView(m_depthStencil.Get(), &dsvDesc, m_dsvHeapHandle);
}

// ============================================================================================================================= DirectX12

// ============================================================================================================================= DescriptorHeap

DirectX::Descriptor* DirectX::Descriptor::instance = nullptr;

DirectX::Descriptor* DirectX::Descriptor::Get()
{
	if (instance == nullptr)
	{
		instance = new Descriptor();
	}
	return instance;
}

void DirectX::Descriptor::Del()
{
	if (instance != nullptr)
	{
		delete instance;
		instance = nullptr;
	}
}

bool DirectX::Descriptor::Init(UINT capacity)
{
	m_capacity = capacity;

	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.NumDescriptors = capacity;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	/*
	* D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
	* 3種類のリソースビューをまとめて管理できるDescriptorHeapを作成するためのフラグ
	* CBV
	*	シェーダーへの定数バッファ
	*		ワールド行列
	*		ビュー・プロジェクション行列
	*		ライト情報
	*		マテリアル情報
	* SRV
	*	シェーダーが読み込むリソース
	*		テクスチャ
	*		構造化バッファ
	*		Shadowマップ
	* UAV
	*	シェーダーが読み書きできる
	*		コンピュートシェーダーの出力
	*		パーティクル更新バッファ
	*		スキニング結果バッファ
	*		スクリーンスペース効果
	*/

	HRESULT hr;
	hr = DirectX::DirectX12::Get()->GetDevice()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_heap));
	if (FAILED(hr))
	{
		MessageBox(nullptr, "DescriptorHeapの作成に失敗しました", "DirectX12の初期化エラー", MB_OK);
		return false;
	};

	m_descriptorSize = DirectX::DirectX12::Get()->GetDevice()->GetDescriptorHandleIncrementSize(desc.Type);

	for (UINT i = 0; i < capacity; i++)
	{
		m_freeQueue.push(i);
	}
	m_allocated.assign(capacity, false);
	/*
	* capacity個をfalseで上書きする
	*/

	return true;
}

UINT DirectX::Descriptor::Allocate()
{
	if (m_freeQueue.empty())
	{
		return UINT_MAX;
	}
	UINT index = m_freeQueue.front();
	m_freeQueue.pop();
	m_allocated[index] = true;
	return index;
}

void DirectX::Descriptor::Free(UINT index)
{
	if (index >= m_capacity)return;
	if (!m_allocated[index])return;
	m_allocated[index] = false;
	m_freeQueue.push(index);
}

D3D12_CPU_DESCRIPTOR_HANDLE DirectX::Descriptor::GetCPUHandle(UINT index) const
{
	if (index >= m_capacity)return { 0 };
	auto handle = m_heap->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += (SIZE_T)index * m_descriptorSize;
	return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE DirectX::Descriptor::GetGPUHandle(UINT index) const
{
	if (index >= m_capacity)return { 0 };
	auto handle = m_heap->GetGPUDescriptorHandleForHeapStart();
	handle.ptr += (UINT64)index * m_descriptorSize;
	return handle;
}

// ============================================================================================================================= DescriptorHeap

// 頂点バッファをGPUに転送する関数
Microsoft::WRL::ComPtr<ID3D12Resource> DirectX::CreateUploadBuffer(size_t size, const void* initData)
{
	Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer;

	// 1.ヒープの種類を設定
	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;// CPUからGPUへの転送に使用するヒープ

	D3D12_RESOURCE_DESC desc = {};
	// リソースの種類を設定
	/*
	* D3D12_RESOURCE_DIMENSION_BUFFER 1次元の線形バッファ（頂点バッファ、定数バッファ、構造化バッファ)
	*				_DIMENSION_TEXTURE1D 1Dテクスチャ
	*				_DIMENSION_TEXTURE2D 2Dテクスチャ
	*				_DIMENSION_TEXTURE3D 3Dテクスチャ
	*/
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Width = size; // バッファのサイズ
	desc.Height = 1; // バッファは1行
	desc.DepthOrArraySize = 1; // バッファは1枚
	desc.MipLevels = 1; // ミップマップレベルは1
	desc.Format = DXGI_FORMAT_UNKNOWN; // バッファはフォーマットなし
	desc.SampleDesc.Count = 1; // マルチサンプリングなし
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; // バッファは行優先
	desc.Flags = D3D12_RESOURCE_FLAG_NONE; // フラグなし

	HRESULT hr = DirectX::DirectX12::Get()->GetDevice()->CreateCommittedResource(
		&heapProps, // ヒープのプロパティ
		D3D12_HEAP_FLAG_NONE, // ヒープのフラグ
		&desc, // リソースの説明
		D3D12_RESOURCE_STATE_GENERIC_READ, // 初期状態は読み取り可能
		nullptr, // 最適化されたクリア値はなし
		IID_PPV_ARGS(&uploadBuffer) // 結果のリソースを受け取る
	);

	if (FAILED(hr))
	{
		return nullptr;
	}

	if (initData != nullptr)
	{
		void* mapped = nullptr;
		uploadBuffer->Map(0, nullptr, &mapped); //GPUメモリをCPUからアクセス可能にする
		memcpy(mapped, initData, size); // 初期データをコピー
		uploadBuffer->Unmap(0, nullptr); // マッピングを解除
	}
	return uploadBuffer;
}