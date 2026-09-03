#pragma once


#include <d3d12.h>
#include <d3d12shader.h>   // ID3D12ShaderReflection / D3D12_SHADER_INPUT_BIND_DESC
#include <d3dcompiler.h>   // D3DCompileFromFile / D3DReflect
#include <wrl/client.h>

#include <string>
#include <vector>
#include <map>

#include "ShaderLayout.h"  // VertexType / ShaderLayout::*InputLayout

using Microsoft::WRL::ComPtr;


struct SlotInfo
{
	UINT index = 0;
	std::string name;
};

struct ShaderCache
{
	ComPtr<ID3D12RootSignature> RootSignature;
	ComPtr<ID3D12PipelineState> PSO;

	std::vector<SlotInfo> cbvSlots; // constant buffer views, in root-parameter order
	std::vector<SlotInfo> srvSlots; // shader resource views, in root-parameter order

	std::vector<D3D12_SHADER_INPUT_BIND_DESC> vsBindDesc; // raw VS reflection binds
	std::vector<D3D12_SHADER_INPUT_BIND_DESC> psBindDesc; // raw PS reflection binds
};

class Shader
{
public:
	static Shader* Get();
	static void Del();

public:

	bool Load(std::string vsPath, std::string psPath, VertexType vertexType = VertexType::Default);

	ShaderCache GetShaderCache(std::string vsPath, std::string psPath);
	ID3D12PipelineState* GetPSO(std::string vsPath, std::string psPath);
	ID3D12RootSignature* GetRootSignature(std::string vsPath, std::string psPath);
	std::vector<ShaderCache> GetAllShaderCache();

private:
	bool CompileShader(const std::string& vsPath, const std::string& psPath,
		ComPtr<ID3DBlob>& vsBlob, ComPtr<ID3DBlob>& psBlob);

	bool CreateRootSignature(const ComPtr<ID3DBlob>& vsBlob, const ComPtr<ID3DBlob>& psBlob,
		ComPtr<ID3D12RootSignature>& rootSignature,
		std::vector<SlotInfo>& cbvSlots, std::vector<SlotInfo>& srvSlots,
		ShaderCache& cache);

	bool CreatePSO(ID3DBlob* vsBlob, ID3DBlob* psBlob,
		ID3D12RootSignature* rootSignature,
		ComPtr<ID3D12PipelineState>& pso, VertexType vertexType);

private:
	Shader() = default;
	~Shader() = default;

private:
	std::map<std::string, ShaderCache> m_cache;
	static Shader* m_instance;
};
