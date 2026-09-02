#pragma once
#include <d3d12.h>
#include <DirectXMath.h>

enum class VertexType
{
	Default,	// デフォルト頂点
	Skinned,	// スキン頂点
	Sprite,		// 2D/UI/スプライト頂点
	Instance,	// インスタンス頂点(GPUインスタンシング用)
	Terrian,	// 地形頂点
	Debug,		// デバッグ頂点
};

namespace ShaderLayout
{
	struct Default
	{
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT3 normal;
		DirectX::XMFLOAT2 uv;
	};

	struct Skinned
	{
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT3 normal;
		DirectX::XMFLOAT2 uv;
		DirectX::XMUINT4 boneIndex;
		DirectX::XMFLOAT4 boneWeight;
	};

	struct Sprite
	{
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT2 uv;
	};

	struct Terrian
	{
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT3 normal;
		DirectX::XMFLOAT2 uv;
		DirectX::XMFLOAT4 splatWeight;
	};

	struct Debug
	{
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT4 color;
	};

	struct TransfromCB
	{
		DirectX::XMFLOAT4X4 world;
		DirectX::XMFLOAT4X4 view;
		DirectX::XMFLOAT4X4 projection;
	};

	// ノードグラフの評価結果をピクセルシェーダーに渡すための定数バッファ(Model_PS.hlslのregister(b1)に対応)
	struct NodeGraphParamsCB
	{
		DirectX::XMFLOAT4 tintColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		int tintMode = 0; // 0 = 適用しない, 1 = 加算, 2 = 乗算, 3 = Lerp
		float lerpT = 0.5f;
		DirectX::XMFLOAT2 pad = { 0.0f, 0.0f };
	};

	// スペキュラ・リムライト計算用の定数バッファ(Model_PS.hlslのregister(b2)に対応)
	struct LightingParamsCB
	{
		DirectX::XMFLOAT3 cameraPos = { 0.0f, 0.0f, 0.0f };
		float specularPower = 32.0f;
		DirectX::XMFLOAT3 lightDir = { 0.4f, 0.8f, -0.4f }; // 正規化前でよい(シェーダー側で正規化する)
		float lightPad = 0.0f;
	};

	static const D3D12_INPUT_ELEMENT_DESC DefaultInputLayout[] =
	{
		{"POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL",		0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD",	0, DXGI_FORMAT_R32G32_FLOAT,	0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};

	static const D3D12_INPUT_ELEMENT_DESC SkinnedInputLayout[] =
	{
		{"POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL",		0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD",	0, DXGI_FORMAT_R32G32_FLOAT,	0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"BONEINDEX",	0, DXGI_FORMAT_R32G32B32A32_UINT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"BONEWEIGHT",	0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};

	static const D3D12_INPUT_ELEMENT_DESC SpriteInputLayout[] =
	{
		{"POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD",	0, DXGI_FORMAT_R32G32_FLOAT,	0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};

	static const D3D12_INPUT_ELEMENT_DESC TerrianInputLayout[] =
	{
		{"POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL",		0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD",	0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"SPLATWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};

	static const D3D12_INPUT_ELEMENT_DESC DebugInputLayout[] =
	{
		{"POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"COLOR",		0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};
}