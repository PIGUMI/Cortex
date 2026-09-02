#pragma once
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/*
	Class   : Shader
	Kind    : Singleton
	Summary : Compiles an HLSL shader pair (VS/PS), then builds and caches the
	          matching RootSignature and PipelineStateObject from shader
	          reflection data. Cache entries are keyed by (vsPath + psPath).
	Author  : Garu
	Updated : 2026/09/02
*/
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
// NOTE: ASCII-only comments on purpose - this header is included from both
//       Shift-JIS (.cpp) and UTF-8 translation units.

#include <d3d12.h>
#include <d3d12shader.h>   // ID3D12ShaderReflection / D3D12_SHADER_INPUT_BIND_DESC
#include <d3dcompiler.h>   // D3DCompileFromFile / D3DReflect
#include <wrl/client.h>

#include <string>
#include <vector>
#include <map>

#include "ShaderLayout.h"  // VertexType / ShaderLayout::*InputLayout

using Microsoft::WRL::ComPtr;

// One HLSL resource (a CBV or an SRV) paired with the root-parameter index it
// was assigned to. 'name' is the resource name taken from shader reflection.
struct SlotInfo
{
	UINT index = 0;
	std::string name;
};

// Everything produced from a single VS+PS pair. Copyable (ComPtr is
// reference counted), so the getters can hand it back by value.
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
	// Compile the VS/PS pair at the given paths and build its RootSignature and
	// PSO for the requested vertex layout. Cached by (vsPath + psPath); calling
	// again with the same pair is a no-op that returns true.
	bool Load(std::string vsPath, std::string psPath, VertexType vertexType);

	// Cache lookups keyed by (vsPath + psPath). The pointer getters return
	// nullptr when the pair has not been Load()ed.
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
