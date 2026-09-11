#include "RenderInterface_DX12.h"
#include "DirectX12.h"
#include "Texture.h"     // Texture::DefaultWhiteKey (無地/無テクスチャ描画のフォールバック)
#include <d3dx12.h>
#include <d3dcompiler.h>
#include <DirectXTex.h>
#include <cstring>

#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;

namespace
{
	// D3D12_TEXTURE_DATA_PITCH_ALIGNMENT 等は UpdateSubresources (d3dx12.h) が内部で処理するので
	// ここでは幅*4 のタイトパックで CPU バッファを用意すればよい
	void PremultiplyAndRepack(const DirectX::Image& src, std::vector<uint8_t>& outTightRGBA8)
	{
		outTightRGBA8.resize(src.width * src.height * 4);
		for (size_t y = 0; y < src.height; ++y)
		{
			const uint8_t* srcRow = src.pixels + y * src.rowPitch;
			uint8_t* dstRow = outTightRGBA8.data() + y * src.width * 4;
			for (size_t x = 0; x < src.width; ++x)
			{
				const uint8_t a = srcRow[x * 4 + 3];
				dstRow[x * 4 + 0] = static_cast<uint8_t>((srcRow[x * 4 + 0] * a) / 255);
				dstRow[x * 4 + 1] = static_cast<uint8_t>((srcRow[x * 4 + 1] * a) / 255);
				dstRow[x * 4 + 2] = static_cast<uint8_t>((srcRow[x * 4 + 2] * a) / 255);
				dstRow[x * 4 + 3] = a;
			}
		}
	}
}

RenderInterface_DX12::~RenderInterface_DX12()
{
	Shutdown();
}

bool RenderInterface_DX12::Init()
{
	// 無テクスチャ描画 (texture == 0) 時のフォールバックとして白 1x1 を保証しておく。
	// LoadDefaults() はアップロードを予約するだけで実際の転送は Flush() まで行われない。
	// ここで明示的に Flush() しておかないと、初回描画時に RenderGeometry() 内の
	// GetSRVGpuHandle() が遅延 Flush() を発火させてしまい、BeginDraw() が積んでいる
	// 描画中の Direct コマンドリストを Texture::Flush() が丸ごとリセットしてしまう
	// (RTV/ビューポート設定やバックバッファの状態遷移が消え、描画が壊れる)
	Texture::Get()->LoadDefaults();
	Texture::Get()->Flush();

	m_projectionCB = DirectX::CreateUploadBuffer(256, nullptr);
	if (m_projectionCB)
	{
		m_projectionCB->Map(0, nullptr, &m_projectionMapped);
	}

	return CreateRootSignatureAndPSO();
}

void RenderInterface_DX12::Shutdown()
{
	for (auto& pair : m_textures)
	{
		if (pair.second.descriptorIndex != UINT_MAX)
		{
			DirectX::Descriptor::Get()->Free(pair.second.descriptorIndex);
		}
	}
	m_textures.clear();
	m_geometry.clear();
	m_pendingUploadBuffers.clear();

	if (m_projectionCB)
	{
		m_projectionCB->Unmap(0, nullptr);
		m_projectionMapped = nullptr;
		m_projectionCB.Reset();
	}
	m_pso.Reset();
	m_rootSignature.Reset();
	m_commandList = nullptr;
}

void RenderInterface_DX12::BeginFrame(ID3D12GraphicsCommandList* commandList, UINT width, UINT height)
{
	// 前フレームで積んだアップロード用中継バッファを解放する。
	// 直前の DirectX12::EndDraw() が GPU 完了を待ってから戻ってきているので、ここで解放して安全
	m_pendingUploadBuffers.clear();

	m_commandList = commandList;
	m_width = width;
	m_height = height;
	m_scissorEnabled = false;

	if (!m_whiteTextureReady)
	{
		const uint8_t whitePixel[4] = { 255, 255, 255, 255 };
		m_whiteTextureReady = UploadTexture2D(whitePixel, 1, 1, m_whiteTexture);
	}

	if (m_projectionMapped && width > 0 && height > 0)
	{
		using namespace DirectX;
		// スクリーン座標系: 原点は左上、+Y が下向き (RmlUi の座標系そのまま)。
		// HLSL 側は cbuffer をデフォルト (column-major) パッキングで読むため、それ自体が
		// 暗黙の転置になる。ここで明示的に転置すると二重転置になり translation 成分が
		// 誤った位置 (w 成分側) に入ってしまうため、転置せずそのまま (row-major) 書き込む。
		XMMATRIX proj = XMMatrixOrthographicOffCenterLH(0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 0.0f, 1.0f);
		XMFLOAT4X4 projT;
		XMStoreFloat4x4(&projT, proj);
		std::memcpy(m_projectionMapped, &projT, sizeof(projT));
	}
}

bool RenderInterface_DX12::CreateRootSignatureAndPSO()
{
	ID3D12Device* device = DirectX::DirectX12::Get()->GetDevice();

	// --- RootSignature ---
	// b0: projection (フレームに1回) / b1: translation (描画のたびにルート定数) / t0: テクスチャ SRV
	CD3DX12_DESCRIPTOR_RANGE srvRange;
	srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_ROOT_PARAMETER params[3];
	params[0].InitAsConstantBufferView(0);
	params[1].InitAsConstants(2, 1);
	params[2].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_PIXEL);

	D3D12_STATIC_SAMPLER_DESC sampler = {};
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.ShaderRegister = 0;
	sampler.RegisterSpace = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	CD3DX12_ROOT_SIGNATURE_DESC rsDesc(
		_countof(params), params,
		1, &sampler,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	ComPtr<ID3DBlob> sigBlob, errorBlob;
	HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errorBlob);
	if (FAILED(hr))
	{
		if (errorBlob) MessageBoxA(nullptr, static_cast<char*>(errorBlob->GetBufferPointer()), "RmlUi RootSignature", MB_OK | MB_ICONERROR);
		return false;
	}
	hr = device->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature));
	if (FAILED(hr))
	{
		return false;
	}

	// --- シェーダー ---
	// Shader/Material のリフレクション経由のパイプラインとは別物 (2D 専用の手組み RootSignature) なので、
	// Cortex の Shader クラスは使わずここで直接コンパイルする
	const UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
	ComPtr<ID3DBlob> vsBlob, psBlob, compileError;

	hr = D3DCompileFromFile(L"Hlsl/RmlUi_VS.hlsl", nullptr, nullptr, "main", "vs_5_0", compileFlags, 0, &vsBlob, &compileError);
	if (FAILED(hr))
	{
		if (compileError) MessageBoxA(nullptr, static_cast<char*>(compileError->GetBufferPointer()), "RmlUi VS Compile", MB_OK | MB_ICONERROR);
		return false;
	}
	hr = D3DCompileFromFile(L"Hlsl/RmlUi_PS.hlsl", nullptr, nullptr, "main", "ps_5_0", compileFlags, 0, &psBlob, &compileError);
	if (FAILED(hr))
	{
		if (compileError) MessageBoxA(nullptr, static_cast<char*>(compileError->GetBufferPointer()), "RmlUi PS Compile", MB_OK | MB_ICONERROR);
		return false;
	}

	// --- PSO ---
	// Rml::Vertex = { Vector2f position; ColourbPremultiplied colour(RGBA8); Vector2f tex_coord; } (20 bytes)
	const D3D12_INPUT_ELEMENT_DESC layout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 8,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	D3D12_BLEND_DESC blendDesc = {};
	// 頂点色・テクスチャとも premultiplied alpha 前提
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	D3D12_RASTERIZER_DESC rasterDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	rasterDesc.CullMode = D3D12_CULL_MODE_NONE;
	// D3D12 の D3D12_RASTERIZER_DESC に ScissorEnable は存在しない (D3D11 のみ)。
	// シザーは RSSetScissorRects で常に有効なので、無効時は毎描画で画面全体の矩形を渡す (RenderGeometry 側で対応)

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = m_rootSignature.Get();
	psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
	psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
	psoDesc.InputLayout = { layout, _countof(layout) };
	psoDesc.RasterizerState = rasterDesc;
	psoDesc.BlendState = blendDesc;
	psoDesc.DepthStencilState.DepthEnable = FALSE; // 2D UI に深度は不要 (重ね順は描画順で決まる)
	psoDesc.DepthStencilState.StencilEnable = FALSE;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.SampleDesc.Count = 1;

	hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pso));
	return SUCCEEDED(hr);
}

bool RenderInterface_DX12::UploadTexture2D(const void* pixels, UINT width, UINT height, TextureEntry& outEntry)
{
	if (!m_commandList || width == 0 || height == 0)
	{
		return false;
	}

	ID3D12Device* device = DirectX::DirectX12::Get()->GetDevice();

	D3D12_RESOURCE_DESC texDesc = {};
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Width = width;
	texDesc.Height = height;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

	CD3DX12_HEAP_PROPERTIES defaultHeap(D3D12_HEAP_TYPE_DEFAULT);
	HRESULT hr = device->CreateCommittedResource(
		&defaultHeap, D3D12_HEAP_FLAG_NONE, &texDesc,
		D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&outEntry.texture)
	);
	if (FAILED(hr))
	{
		return false;
	}

	UINT64 uploadSize = 0;
	device->GetCopyableFootprints(&texDesc, 0, 1, 0, nullptr, nullptr, nullptr, &uploadSize);

	ComPtr<ID3D12Resource> uploadBuffer;
	CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadSize);
	hr = device->CreateCommittedResource(
		&uploadHeap, D3D12_HEAP_FLAG_NONE, &uploadDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadBuffer)
	);
	if (FAILED(hr))
	{
		return false;
	}

	D3D12_SUBRESOURCE_DATA sub = {};
	sub.pData = pixels;
	sub.RowPitch = static_cast<LONG_PTR>(width) * 4;
	sub.SlicePitch = sub.RowPitch * height;

	// このフレームの Direct コマンドリストに直接コピーを積む。
	// Texture マネージャの Copy キュー + Flush(内部で Direct をリセットして即待機) は、
	// フレーム中盤で呼ぶと「積みかけの描画コマンドが消える」危険があるため使わない。
	UpdateSubresources(m_commandList, outEntry.texture.Get(), uploadBuffer.Get(), 0, 0, 1, &sub);
	m_pendingUploadBuffers.push_back(uploadBuffer); // GPU 転送完了 (次フレーム冒頭) まで保持

	const D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		outEntry.texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
	);
	m_commandList->ResourceBarrier(1, &barrier);

	outEntry.descriptorIndex = DirectX::Descriptor::Get()->Allocate();
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = texDesc.Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = 1;
	device->CreateShaderResourceView(outEntry.texture.Get(), &srvDesc, DirectX::Descriptor::Get()->GetCPUHandle(outEntry.descriptorIndex));

	return true;
}

Rml::CompiledGeometryHandle RenderInterface_DX12::CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices)
{
	CompiledGeometry geometry;

	const size_t vbSize = vertices.size() * sizeof(Rml::Vertex);
	const size_t ibSize = indices.size() * sizeof(int);

	geometry.vertexBuffer = DirectX::CreateUploadBuffer(vbSize, vertices.data());
	geometry.indexBuffer = DirectX::CreateUploadBuffer(ibSize, indices.data());
	if (!geometry.vertexBuffer || !geometry.indexBuffer)
	{
		return 0;
	}

	geometry.vbv.BufferLocation = geometry.vertexBuffer->GetGPUVirtualAddress();
	geometry.vbv.SizeInBytes = static_cast<UINT>(vbSize);
	geometry.vbv.StrideInBytes = sizeof(Rml::Vertex);

	geometry.ibv.BufferLocation = geometry.indexBuffer->GetGPUVirtualAddress();
	geometry.ibv.SizeInBytes = static_cast<UINT>(ibSize);
	geometry.ibv.Format = DXGI_FORMAT_R32_UINT;
	geometry.indexCount = static_cast<UINT>(indices.size());

	const uint64_t id = m_nextGeometryId++;
	m_geometry.emplace(id, std::move(geometry));
	return static_cast<Rml::CompiledGeometryHandle>(id);
}

void RenderInterface_DX12::RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture)
{
	if (!m_commandList)
	{
		return;
	}

	const auto it = m_geometry.find(static_cast<uint64_t>(geometry));
	if (it == m_geometry.end())
	{
		return;
	}
	const CompiledGeometry& g = it->second;

	m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
	m_commandList->SetPipelineState(m_pso.Get());

	if (m_projectionCB)
	{
		m_commandList->SetGraphicsRootConstantBufferView(0, m_projectionCB->GetGPUVirtualAddress());
	}
	const float translationConsts[2] = { translation.x, translation.y };
	m_commandList->SetGraphicsRoot32BitConstants(1, 2, translationConsts, 0);

	D3D12_GPU_DESCRIPTOR_HANDLE srv;
	const auto texIt = m_textures.find(static_cast<uint64_t>(texture));
	if (texture != 0 && texIt != m_textures.end())
	{
		srv = DirectX::Descriptor::Get()->GetGPUHandle(texIt->second.descriptorIndex);
	}
	else
	{
		// テクスチャ無し描画: 白 1x1 を張ることで「サンプル結果 * 頂点色」を頂点色そのものにする
		srv = DirectX::Descriptor::Get()->GetGPUHandle(m_whiteTexture.descriptorIndex);
	}
	m_commandList->SetGraphicsRootDescriptorTable(2, srv);

	const D3D12_RECT scissor = m_scissorEnabled
		? m_scissorRect
		: D3D12_RECT{ 0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height) };
	m_commandList->RSSetScissorRects(1, &scissor);

	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_commandList->IASetVertexBuffers(0, 1, &g.vbv);
	m_commandList->IASetIndexBuffer(&g.ibv);
	m_commandList->DrawIndexedInstanced(g.indexCount, 1, 0, 0, 0);
}

void RenderInterface_DX12::ReleaseGeometry(Rml::CompiledGeometryHandle geometry)
{
	m_geometry.erase(static_cast<uint64_t>(geometry));
}

Rml::TextureHandle RenderInterface_DX12::LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source)
{
	const std::wstring wPath(source.begin(), source.end());

	DirectX::ScratchImage image;
	HRESULT hr = DirectX::LoadFromWICFile(wPath.c_str(), DirectX::WIC_FLAGS_NONE, nullptr, image);
	if (FAILED(hr))
	{
		return 0;
	}

	const DirectX::Image* img = image.GetImage(0, 0, 0);
	if (!img)
	{
		return 0;
	}

	DirectX::ScratchImage converted;
	if (img->format != DXGI_FORMAT_R8G8B8A8_UNORM)
	{
		hr = DirectX::Convert(*img, DXGI_FORMAT_R8G8B8A8_UNORM, DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, converted);
		if (FAILED(hr))
		{
			return 0;
		}
		img = converted.GetImage(0, 0, 0);
	}

	// RmlUi は premultiplied alpha 前提だが、一般的な画像ファイルは straight alpha なのでここで乗算する
	std::vector<uint8_t> premultiplied;
	PremultiplyAndRepack(*img, premultiplied);

	TextureEntry entry;
	if (!UploadTexture2D(premultiplied.data(), static_cast<UINT>(img->width), static_cast<UINT>(img->height), entry))
	{
		return 0;
	}

	texture_dimensions.x = static_cast<int>(img->width);
	texture_dimensions.y = static_cast<int>(img->height);

	const uint64_t id = m_nextTextureId++;
	m_textures.emplace(id, std::move(entry));
	return static_cast<Rml::TextureHandle>(id);
}

Rml::TextureHandle RenderInterface_DX12::GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions)
{
	// source は既に RGBA8 premultiplied alpha (RenderInterface.h のコメントより)。変換不要でそのままアップロード
	TextureEntry entry;
	if (!UploadTexture2D(source.data(), static_cast<UINT>(source_dimensions.x), static_cast<UINT>(source_dimensions.y), entry))
	{
		return 0;
	}

	const uint64_t id = m_nextTextureId++;
	m_textures.emplace(id, std::move(entry));
	return static_cast<Rml::TextureHandle>(id);
}

void RenderInterface_DX12::ReleaseTexture(Rml::TextureHandle texture)
{
	const auto it = m_textures.find(static_cast<uint64_t>(texture));
	if (it == m_textures.end())
	{
		return;
	}
	if (it->second.descriptorIndex != UINT_MAX)
	{
		DirectX::Descriptor::Get()->Free(it->second.descriptorIndex);
	}
	m_textures.erase(it);
}

void RenderInterface_DX12::EnableScissorRegion(bool enable)
{
	m_scissorEnabled = enable;
}

void RenderInterface_DX12::SetScissorRegion(Rml::Rectanglei region)
{
	m_scissorRect.left = region.Left();
	m_scissorRect.top = region.Top();
	m_scissorRect.right = region.Left() + region.Width();
	m_scissorRect.bottom = region.Top() + region.Height();
}
