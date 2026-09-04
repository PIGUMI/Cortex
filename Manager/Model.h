#pragma once
#include "ShaderLayout.h"
#include "Material.h"
#include <string>
#include <vector>

/*
* Model
* モデルファイル (assimp 経由) を読み込み、サブメッシュ単位で
* ジオメトリのバッファビュー + マテリアルを保持する「アセット」。
* インスタンス状態 (transform / 有効フラグの実体 / 定数バッファ) は
* MeshRender コンポーネント側が持つ。
*/

struct SubMesh
{
	bool                     isActive = true;
	std::string              MeshName;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	D3D12_INDEX_BUFFER_VIEW  indexBufferView{};
	UINT                     IndexCount = 0;   // DrawIndexedInstanced 用 (indices.size())
	Material                 material;         // モデルファイル由来の既定マテリアル (値保持)
};

class Model
{
public:
	/**
	 * @brief モデルを読み込み、各サブメッシュのマテリアルを生成する
	 * @param modelPath        モデルファイルのパス
	 * @param pixelShaderPath  ピクセルシェーダーのパス
	 * @param vertexShaderPath 頂点シェーダーのパス
	 * @param vertexType       頂点レイアウト
	 * @return いずれかのサブメッシュのマテリアル生成に失敗したら false
	 */
	bool CreateModel(const std::string& modelPath,
	                 const std::string& pixelShaderPath  = "Hlsl/Model_PS.hlsl",
	                 const std::string& vertexShaderPath = "Hlsl/Model_VS.hlsl",
	                 VertexType vertexType = VertexType::Default);

	bool IsLoaded() const { return m_loaded; }

	const std::vector<SubMesh>& GetSubMeshes() const { return m_subMeshes; }

private:
	std::vector<SubMesh> m_subMeshes;
	bool m_loaded = false;
};
