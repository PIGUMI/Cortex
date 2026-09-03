#include "Material.h"
#include "Texture.h"

using namespace std;

bool Material::Create(const string& materialName, const string& vertexPath, const string& pixelPath, VertexType vertexType)
{
	// シェーダーのコンパイルと PSO / RootSignature の生成 (パスをキーにキャッシュされる)
	if (!Shader::Get()->Load(vertexPath, pixelPath, vertexType))
	{
		return false;
	}

	m_materialName     = materialName;
	m_vertexShaderPath = vertexPath;
	m_pixelShaderPath  = pixelPath;
	m_vertexType       = vertexType;
	m_shaderCache      = Shader::Get()->GetShaderCache(vertexPath, pixelPath);

	// シェーダーが要求する SRV の数だけテクスチャスロットを用意し、
	// 既定値としてダミーの白テクスチャを割り当てる。srvSlots[i].index は
	// そのリソースが割り当てられたルートパラメータ番号そのもの。
	m_textureData.clear();
	m_textureData.reserve(m_shaderCache.srvSlots.size());
	for (const auto& srv : m_shaderCache.srvSlots)
	{
		m_textureData.push_back({ srv.name, srv.index, Texture::DefaultWhiteKey });
	}

	m_isCreated = true;
	return true;
}

void Material::Bind(ID3D12GraphicsCommandList* commandList) const
{
	if (!m_isCreated || commandList == nullptr)
	{
		return;
	}

	commandList->SetGraphicsRootSignature(m_shaderCache.RootSignature.Get());
	commandList->SetPipelineState(m_shaderCache.PSO.Get());

	// 各テクスチャスロットへ SRV ディスクリプタテーブルを設定する。
	// CBV は毎フレーム / オブジェクトごとに呼び出し側が設定する想定なのでここでは触らない。
	for (const auto& tex : m_textureData)
	{
		commandList->SetGraphicsRootDescriptorTable(
			tex.RootParamIndex,
			Texture::Get()->GetSRVGpuHandle(tex.TexturePath));
	}
}

void Material::SetTexturePath(UINT slotIndex, const string& texturePath)
{
	if (slotIndex >= m_textureData.size())
	{
		return;
	}

	// 空文字列、または読み込みに失敗したパスはダミーの白テクスチャにフォールバックする。
	if (texturePath.empty() || !Texture::Get()->Load(texturePath))
	{
		m_textureData[slotIndex].TexturePath = Texture::DefaultWhiteKey;
		return;
	}

	m_textureData[slotIndex].TexturePath = texturePath;
}
