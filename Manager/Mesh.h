#pragma once
#include "ShaderLayout.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <wrl/client.h>

using namespace Microsoft::WRL;

struct TexturePath
{
	std::string Diffuse;
	std::string Normal;
	std::string Specular;
	std::string Emissive;
	std::string Metalness;
	std::string AmbientOcclusion;
};

struct ISubMesh
{
	/**
	 * @brief サブメッシュの名前
	 */
	std::string SubMeshName = "None";
	/**
	 * @brief サブメッシュの頂点データ
	 */
	std::vector<ShaderLayout::Default> vertices;
	/**
	 * @brief サブメッシュのインデックスデータ
	 */
	std::vector<uint32_t> indices;
	/**
	 * @brief サブメッシュの頂点バッファ
	 */
	ComPtr<ID3D12Resource> vertexBuffer;
	/**
	 * @brief サブメッシュの頂点バッファビュー
	 */
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	/**
	 * @brief サブメッシュのインデックスバッファ
	 */
	ComPtr<ID3D12Resource> indexBuffer;
	/**
	 * @brief サブメッシュのインデックスバッファビュー
	 */
	D3D12_INDEX_BUFFER_VIEW indexBufferView;
	/**
	 * @brief サブメッシュのテクスチャパス
	 */
	TexturePath texturePath;
};

struct IMesh
{
	std::vector<ISubMesh> subMeshes;
};

class Mesh
{
public:
	static Mesh* Get();
	static void Del();

	bool Load(const std::string& FILEPATH);

	const IMesh* GetMeshData(const std::string& name);
private:
	std::unordered_map<std::string, IMesh> m_meshMap;
private:
	static Mesh* instance;
};
