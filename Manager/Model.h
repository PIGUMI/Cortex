#pragma once
#include "ShaderLayout.h"
#include "Material.h"
#include <string>
#include <vector>

struct SubMesh
{
	bool isActive = true;
	std::string MeshName;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	D3D12_INDEX_BUFFER_VIEW indexBufferView;
	Material* material;
};

class Model
{
public:
	Model();
	~Model();

	bool CreateModel(const std::string& modelPath, const std::string& pixelShaderPath = "Hlsl/Model_PS.hlsl", VertexType vertexType = VertexType::Default);
	//void InfoPrintByImGui();
	std::vector<SubMesh> GetSubMeshes() { return m_subMeshes; }
	void SetSubMeshes(std::vector<SubMesh> subMeshes) { m_subMeshes = subMeshes; }
private:
	std::vector<SubMesh> m_subMeshes;
};
