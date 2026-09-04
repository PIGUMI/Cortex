#include "Model.h"
#include "Mesh.h"
#include <utility>   // std::move

bool Model::CreateModel(const std::string& modelPath, const std::string& pixelShaderPath,
                        const std::string& vertexShaderPath, VertexType vertexType)
{
	if (!Mesh::Get()->Load(modelPath))
	{
		return false;
	}

	const IMesh* meshData = Mesh::Get()->GetMeshData(modelPath);
	if (!meshData)
	{
		return false;
	}

	m_subMeshes.clear();
	m_subMeshes.reserve(meshData->subMeshes.size());

	for (const auto& src : meshData->subMeshes)
	{
		SubMesh sm;
		sm.MeshName         = src.SubMeshName;
		sm.vertexBufferView = src.vertexBufferView;
		sm.indexBufferView  = src.indexBufferView;
		sm.IndexCount       = static_cast<UINT>(src.indices.size());

		if (!sm.material.Create(src.SubMeshName, vertexShaderPath, pixelShaderPath, vertexType))
		{
			// 1 つでも失敗したら中途半端なモデルを残さない
			m_subMeshes.clear();
			return false;
		}

		// Model_PS.hlsl が宣言する SRV の並び順に対応 (0:Diffuse 〜 5:AO)。
		// シェーダーが要求しないスロットへの設定は Material 側で無視される。
		sm.material.SetTexturePath(0, src.texturePath.Diffuse);
		sm.material.SetTexturePath(1, src.texturePath.Normal);
		sm.material.SetTexturePath(2, src.texturePath.Specular);
		sm.material.SetTexturePath(3, src.texturePath.Emissive);
		sm.material.SetTexturePath(4, src.texturePath.Metalness);
		sm.material.SetTexturePath(5, src.texturePath.AmbientOcclusion);

		m_subMeshes.push_back(std::move(sm));
	}

	m_loaded = true;
	return true;
}
