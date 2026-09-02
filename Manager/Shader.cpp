#include "Shader.h"
#include "DirectX12.h"

#pragma comment(lib, "d3dcompiler.lib")

Shader* Shader::m_instance = nullptr;

Shader* Shader::Get()
{
	if (m_instance == nullptr)
	{
		m_instance = new Shader();
	}
	return m_instance;
}

void Shader::Del()
{
	if (m_instance != nullptr)
	{
		delete m_instance;
		m_instance = nullptr;
	}
}

bool Shader::Load(std::string vsPath, std::string psPath, VertexType vertexType)
{
	std::string key = vsPath + psPath;

	// 同じパスの組み合わせが既にキャッシュ済みなら再コンパイルしない
	if (m_cache.count(key) > 0)
	{
		// キャッシュ済みなので何もせず成功として返す
		return true;
	}

	ComPtr<ID3DBlob> vsBlob;
	ComPtr<ID3DBlob> psBlob;

	// シェーダーをコンパイル
	if (!CompileShader(vsPath, psPath, vsBlob, psBlob))
	{
		return false;
	}

	ShaderCache cache;
	// ルートシグネチャを作成
	if (!CreateRootSignature(vsBlob.Get(), psBlob.Get(),
		cache.RootSignature, cache.cbvSlots, cache.srvSlots, cache))
	{
		MessageBox(nullptr, "ルートシグネチャの作成に失敗", "エラー", MB_OK);
		return false;
	}

	// PipelineStateObject を作成
	if (!CreatePSO(vsBlob.Get(), psBlob.Get(), cache.RootSignature.Get(), cache.PSO, vertexType))
	{
		MessageBox(nullptr, "PSO の作成に失敗", "エラー", MB_OK);
		return false;
	}

	m_cache[key] = std::move(cache);

	return true;
}

ShaderCache Shader::GetShaderCache(std::string vsPath, std::string psPath)
{
	std::string key = vsPath + psPath;
	auto it = m_cache.find(key);
	if (it == m_cache.end())
	{
		return ShaderCache();
	}
	return it->second;
}

bool Shader::CompileShader(const std::string& vsPath, const std::string& psPath, ComPtr<ID3DBlob>& vsBlob, ComPtr<ID3DBlob>& psBlob)
{
	UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
	/*
	* D3DCOMPILE_DEBUG
	*	デバッグ情報付きでシェーダーをコンパイルするフラグ
	* D3DCOMPILE_SKIP_OPTIMIZATION
	*	最適化をスキップするフラグ
	*	最適化を行わないことでデバッガでの値の確認がしやすくなる
	*/

	ComPtr<ID3DBlob> errorBlob;
	/*
	* ID3DBlob はバイナリデータを保持するための汎用バッファ
	*/

	std::wstring wvsPath(vsPath.begin(), vsPath.end());

	// 頂点シェーダーをコンパイル
	HRESULT hr = D3DCompileFromFile(
		wvsPath.c_str(), nullptr, nullptr,
		"main", "vs_5_0",
		compileFlags, 0,
		&vsBlob, &errorBlob
	);

	if (FAILED(hr))
	{
		return false;
	}

	std::wstring wpsPath(psPath.begin(), psPath.end());

	// ピクセルシェーダーをコンパイル
	hr = D3DCompileFromFile(
		wpsPath.c_str(), nullptr, nullptr,
		"main", "ps_5_0",
		compileFlags, 0,
		&psBlob, &errorBlob
	);

	if (FAILED(hr))
	{
		std::string errorMsg = (char*)errorBlob->GetBufferPointer();
		MessageBox(nullptr, std::string(errorMsg.begin(), errorMsg.end()).c_str(), "ピクセルシェーダーコンパイルエラー", MB_OK);
		return false;
	}

	return true;
}

/*
* RootSignature の作成に必要なデータ
* CD3DX12_ROOT_SIGNATURE_DESC desc(
*  rootParams.size() // rootParameter の数
*  rootParams.data() // rootParameter の配列
*  1                 // staticSampler の数
*  &sampler          // staticSampler の配列
*  flags             // ルートシグネチャのフラグ
* );
*/
bool Shader::CreateRootSignature(const ComPtr<ID3DBlob>& vsBlob, const ComPtr<ID3DBlob>& psBlob, ComPtr<ID3D12RootSignature>& rootSignature, std::vector<SlotInfo>& cbvSlots, std::vector<SlotInfo>& srvSlots, ShaderCache& cache)
{
	/*
	* コンパイル済みシェーダーの内部情報を取得するためのインターフェース
	* シェーダーのバイトコードを解析することで、シェーダーが使用する
	* 入力要素や定数バッファなどの情報を取得できる
	*/
	ComPtr<ID3D12ShaderReflection> vsReflection;

	D3DReflect(
		vsBlob->GetBufferPointer(),
		vsBlob->GetBufferSize(),
		IID_PPV_ARGS(&vsReflection)
	);

	ComPtr<ID3D12ShaderReflection> psReflection;

	D3DReflect(
		psBlob->GetBufferPointer(),
		psBlob->GetBufferSize(),
		IID_PPV_ARGS(&psReflection)
	);

	/*
	* シェーダーの概要情報を表す構造体
	* 　入力パラメーターの数
	* 　出力パラメーターの数
	*	定数バッファの数などが取得できる
	*/
	D3D12_SHADER_DESC vsDesc;
	vsReflection->GetDesc(&vsDesc);

	D3D12_SHADER_DESC psDesc;
	psReflection->GetDesc(&psDesc);

	/*
	* RootSignature に登録するルートパラメーターを定義する構造体
	*/
	std::vector<CD3DX12_ROOT_PARAMETER> rootParams;
	/*
	* DescriptorTable 内の「どのレジスタから何個使うか」を定義する構造体
	*/
	std::vector<CD3DX12_DESCRIPTOR_RANGE> ranges;

	// ルートパラメータとディスクリプタレンジの領域を確保
	UINT tatalResources = vsDesc.BoundResources + psDesc.BoundResources;
	ranges.reserve(tatalResources);
	rootParams.reserve(tatalResources);

	UINT slotIndex = 0;

	//				バインドされているリソースの数だけ回す
	for (UINT i = 0; i < vsDesc.BoundResources; i++)
	{
		/*
		* シェーダーにバインドされているリソース1つの情報を表す構造体
		* Name      : リソースの名前
		* Type      : リソースの種類（定数バッファ・テクスチャなど）
		* BindPoint : シェーダー内のどのスロットにバインドされているか
		* BindCount : 配列要素の数
		* Space     : レジスタスペース（通常0）
		*/
		D3D12_SHADER_INPUT_BIND_DESC bindDesc;
		vsReflection->GetResourceBindingDesc(i, &bindDesc);
		cache.vsBindDesc.push_back(bindDesc);

		switch (bindDesc.Type)
		{
		case D3D_SIT_CBUFFER:
		{
			CD3DX12_ROOT_PARAMETER param;
			param.InitAsConstantBufferView(
				bindDesc.BindPoint, 0,
				D3D12_SHADER_VISIBILITY_VERTEX
			);
			rootParams.push_back(param);

			cbvSlots.push_back({ slotIndex,bindDesc.Name });
			slotIndex++;
			break;
		}
		case D3D_SIT_TEXTURE:
		{
			CD3DX12_DESCRIPTOR_RANGE range;
			range.Init(
				D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
				1,
				bindDesc.BindPoint
			);
			ranges.push_back(range);

			CD3DX12_ROOT_PARAMETER param;
			param.InitAsDescriptorTable(
				1, &ranges.back(),
				D3D12_SHADER_VISIBILITY_PIXEL
			);
			rootParams.push_back(param);
			srvSlots.push_back({ slotIndex,bindDesc.Name });
			slotIndex++;
			break;
		}
		}
	}

	for (UINT i = 0; i < psDesc.BoundResources; i++)
	{
		D3D12_SHADER_INPUT_BIND_DESC bindDesc;
		psReflection->GetResourceBindingDesc(i, &bindDesc);
		cache.psBindDesc.push_back(bindDesc);
		switch (bindDesc.Type)
		{
		case D3D_SIT_CBUFFER:
		{
			CD3DX12_ROOT_PARAMETER param;
			param.InitAsConstantBufferView(
				bindDesc.BindPoint, 0,
				D3D12_SHADER_VISIBILITY_PIXEL
			);
			rootParams.push_back(param);
			cbvSlots.push_back({ slotIndex,bindDesc.Name });
			slotIndex++;
			break;
		}
		case D3D_SIT_TEXTURE:
		{
			CD3DX12_ROOT_PARAMETER param;
			CD3DX12_DESCRIPTOR_RANGE range;
			range.Init(
				D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
				1,
				bindDesc.BindPoint
			);
			ranges.push_back(range);
			param.InitAsDescriptorTable(
				1, &ranges.back(),
				D3D12_SHADER_VISIBILITY_PIXEL
			);
			rootParams.push_back(param);
			// このリソースが実際に積まれた rootParams のインデックス(=slotIndex)を先に記録してからインクリメントする
			// (先にインクリメントすると、次のリソースのインデックスがずれて記録されてしまう)
			srvSlots.push_back({ slotIndex,bindDesc.Name });
			slotIndex++;
			break;
		}
		}
	}

	/* StaticSampler の設定 */
	D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
	samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
	samplerDesc.ShaderRegister = 0;
	samplerDesc.RegisterSpace = 0;
	samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	CD3DX12_ROOT_SIGNATURE_DESC rsDesc(
		(UINT)rootParams.size(),
		rootParams.empty() ? nullptr : rootParams.data(),
		1, &samplerDesc,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	ComPtr<ID3DBlob> sigBlob, errorBlob;
	HRESULT hr = D3D12SerializeRootSignature(
		&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		&sigBlob, &errorBlob
	);
	if (FAILED(hr))
	{
		std::string errorMsg = (char*)errorBlob->GetBufferPointer();
		MessageBox(nullptr, std::string(errorMsg.begin(), errorMsg.end()).c_str(), "ルートシグネチャシリアライズエラー", MB_OK);
		return false;
	}

	hr = DirectX::DirectX12::Get()->GetDevice()->CreateRootSignature(
		0,
		sigBlob->GetBufferPointer(),
		sigBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature)
	);
	return SUCCEEDED(hr);
}

bool Shader::CreatePSO(ID3DBlob* vsBlob, ID3DBlob* psBlob, ID3D12RootSignature* rootSignature, ComPtr<ID3D12PipelineState>& pso, VertexType vertexType)
{
	const D3D12_INPUT_ELEMENT_DESC* layout = nullptr;
	UINT layoutCount = 0;

	switch (vertexType)
	{
	case VertexType::Default:
		layout = ShaderLayout::DefaultInputLayout;
		layoutCount = _countof(ShaderLayout::DefaultInputLayout);
		break;
	case VertexType::Skinned:
		layout = ShaderLayout::SkinnedInputLayout;
		layoutCount = _countof(ShaderLayout::SkinnedInputLayout);
		break;
	case VertexType::Sprite:
		layout = ShaderLayout::SpriteInputLayout;
		layoutCount = _countof(ShaderLayout::SpriteInputLayout);
		break;
	case VertexType::Instance:
		break;
	case VertexType::Terrian:
		layout = ShaderLayout::TerrianInputLayout;
		layoutCount = _countof(ShaderLayout::TerrianInputLayout);
		break;
	case VertexType::Debug:
		layout = ShaderLayout::DebugInputLayout;
		layoutCount = _countof(ShaderLayout::DebugInputLayout);
		break;
	default:
		break;
	}

	D3D12_RASTERIZER_DESC rasterizerDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;

	D3D12_BLEND_DESC blendDesc = {};
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = rootSignature;
	psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
	psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
	psoDesc.InputLayout = { layout, layoutCount };
	psoDesc.RasterizerState = rasterizerDesc;
	psoDesc.BlendState = blendDesc;
	psoDesc.DepthStencilState.DepthEnable = TRUE;
	psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	psoDesc.DepthStencilState.StencilEnable = FALSE;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	// RenderTarget::MSAASampleCount と必ず合わせること(描画先が MSAA テクスチャのため)
	psoDesc.SampleDesc.Count = 4;
	psoDesc.SampleMask = UINT_MAX;

	return SUCCEEDED(
		DirectX::DirectX12::Get()->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso))
	);
}

ID3D12PipelineState* Shader::GetPSO(std::string vsPath, std::string psPath)
{
	std::string key = vsPath + psPath;
	auto it = m_cache.find(key);
	if (it == m_cache.end())
	{
		return nullptr;
	}
	return it->second.PSO.Get();
}

ID3D12RootSignature* Shader::GetRootSignature(std::string vsPath, std::string psPath)
{
	std::string key = vsPath + psPath;
	auto it = m_cache.find(key);
	if (it == m_cache.end())
	{
		return nullptr;
	}
	return it->second.RootSignature.Get();
}

std::vector<ShaderCache> Shader::GetAllShaderCache()
{
	std::vector<ShaderCache> caches;
	for (const auto& pair : m_cache)
	{
		caches.push_back(pair.second);
	}
	return caches;
}
