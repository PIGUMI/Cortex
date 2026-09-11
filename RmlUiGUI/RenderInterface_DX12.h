#pragma once
#include <RmlUi/Core/RenderInterface.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <unordered_map>
#include <vector>
#include <cstdint>

/*
* RenderInterface_DX12
* Rml::RenderInterface の Cortex 向け実装。
* RmlUi 公式の Backends/RmlUi_Renderer_DX12 は自前でデバイス/スワップチェーンを
* 作る自己完結型サンプルなので使わず、Cortex の DirectX12 / Descriptor に
* 相乗りする形で 8 つの必須仮想関数だけを実装する
* (Transform / Layer / Filter / Shader はデフォルト実装のまま = 未対応)。
*
* 使い方:
*   Init()                         起動時 1 回 (DirectX12/Descriptor の Init 後)
*   BeginFrame(cmd, width, height) 毎フレーム、context->Render() を呼ぶ前
*   (Rml::Context::Render() が CompileGeometry/RenderGeometry/... を呼んでくる)
*   Shutdown()                     終了時
*/
class RenderInterface_DX12 : public Rml::RenderInterface
{
public:
	RenderInterface_DX12() = default;
	~RenderInterface_DX12() override;

	bool Init();
	void Shutdown();

	// 毎フレームの開始。以降の CompileGeometry/RenderGeometry はこの cmd に積まれる
	void BeginFrame(ID3D12GraphicsCommandList* commandList, UINT width, UINT height);

	// --- Rml::RenderInterface (必須 8 関数) ---
	Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) override;
	void RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) override;
	void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;

	Rml::TextureHandle LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) override;
	Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions) override;
	void ReleaseTexture(Rml::TextureHandle texture) override;

	void EnableScissorRegion(bool enable) override;
	void SetScissorRegion(Rml::Rectanglei region) override;

private:
	struct CompiledGeometry
	{
		Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer;
		Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer;
		D3D12_VERTEX_BUFFER_VIEW vbv{};
		D3D12_INDEX_BUFFER_VIEW  ibv{};
		UINT indexCount = 0;
	};

	struct TextureEntry
	{
		Microsoft::WRL::ComPtr<ID3D12Resource> texture;
		UINT descriptorIndex = UINT_MAX; // Descriptor シングルトンのヒープ内インデックス
	};

	bool CreateRootSignatureAndPSO();
	bool UploadTexture2D(const void* pixels, UINT width, UINT height, TextureEntry& outEntry);

private:
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pso;

	// projection (1フレームに1回更新)。translation はルート定数で毎描画渡す
	Microsoft::WRL::ComPtr<ID3D12Resource> m_projectionCB;
	void* m_projectionMapped = nullptr;

	ID3D12GraphicsCommandList* m_commandList = nullptr; // 借用。フレームごとに差し替え
	UINT m_width = 0;
	UINT m_height = 0;
	bool m_scissorEnabled = false;
	D3D12_RECT m_scissorRect{};

	uint64_t m_nextGeometryId = 1;
	std::unordered_map<uint64_t, CompiledGeometry> m_geometry;

	uint64_t m_nextTextureId = 1;
	std::unordered_map<uint64_t, TextureEntry> m_textures;

	// texture==0 (無テクスチャ描画) フォールバック用の白 1x1。
	// Texture::DefaultWhiteKey (Texture::BeginUpload 経由、Copy キュー) は D3D12 デバッグレイヤーが
	// バリアレイアウト不整合を報告する既知の問題があるため使わず、UploadTexture2D
	// (Direct キューで正しく遷移する自前の経路) で作り直す
	bool m_whiteTextureReady = false;
	TextureEntry m_whiteTexture;

	// テクスチャアップロード用の中継バッファ。このフレームの Direct コマンドリスト実行完了
	// (= 次の BeginFrame が呼ばれる時点) まで参照を保持しておく必要がある
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_pendingUploadBuffers;
};
