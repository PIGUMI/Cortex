#include "Model.h"
#include "Mesh.h"
#include "Texture.h"
#include "Shader.h"
//#include "GUI.h"

Model::Model()
{
}

Model::~Model()
{
	for (auto& subMesh : m_subMeshes)
	{
		if (subMesh.material)
		{
			delete subMesh.material;
			subMesh.material = nullptr;
		}
	}
	m_subMeshes.clear();
}

bool Model::CreateModel(const std::string& modelPath, const std::string& pixelShaderPath, VertexType vertexType)
{
	if (!Mesh::Get()->Load(modelPath))return false;

	std::vector<ISubMesh> subMeshes = Mesh::Get()->GetMeshData(modelPath)->subMeshes;

	m_subMeshes.clear();

	for (const auto& subMesh : subMeshes)
	{
		SubMesh newSubMesh;
		newSubMesh.MeshName = subMesh.SubMeshName;
		newSubMesh.vertexBufferView = subMesh.vertexBufferView;
		newSubMesh.indexBufferView = subMesh.indexBufferView;
		newSubMesh.material = new Material();
		newSubMesh.material->Create(subMesh.SubMeshName, "Hlsl/Model_VS.hlsl", pixelShaderPath, vertexType);
		newSubMesh.material->SetTexturePath(0, subMesh.texturePath.Diffuse);
		newSubMesh.material->SetTexturePath(1, subMesh.texturePath.Normal);
		newSubMesh.material->SetTexturePath(2, subMesh.texturePath.Specular);
		newSubMesh.material->SetTexturePath(3, subMesh.texturePath.Emissive);
		newSubMesh.material->SetTexturePath(4, subMesh.texturePath.Metalness);
		newSubMesh.material->SetTexturePath(5, subMesh.texturePath.AmbientOcclusion);
		m_subMeshes.push_back(newSubMesh);
	}

	return true;
}

//void Model::InfoPrintByImGui()
//{
//	for (int count = 0; count < m_subMeshes.size(); count++)
//	{
//		SubMesh& subMesh = m_subMeshes[count];
//		if (ImGui::CollapsingHeader(subMesh.MeshName.c_str()))
//		{
//			ImGui::Checkbox(("isActive##" + std::to_string(count)).c_str(), &subMesh.isActive);
//			ImGui::Text("MaterialName: %s", subMesh.material->GetMaterialName().c_str());
//			ImGui::Text("VertexShaderPath: %s", subMesh.material->GetVertexShaderPath().c_str());
//			ImGui::Text("PixelShaderPath: %s", subMesh.material->GetPixelShaderPath().c_str());
//			std::vector<Material::TextureData>* textureData = subMesh.material->GetTextureData();
//			for (int i = 0; i < textureData->size(); i++)
//			{
//				ImGui::Text("SlotName: %s, SlotIndex: %d, TexturePath: %s", (*textureData)[i].SlotName.c_str(), (*textureData)[i].SlotIndex, (*textureData)[i].TexturePath.c_str());
//			}
//		}
//	}
//}