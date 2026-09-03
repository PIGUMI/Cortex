#pragma once
#include "Shader.h"
#include "Texture.h"
#include <vector>
#include <string>
#include <d3d12.h>

/*
* MaterialClass
* シェーダーペア (VS/PS) と、そのシェーダーが要求するテクスチャバインディングを
* ひとまとめにする。PSO / RootSignature は Shader シングルトンのキャッシュを参照する。
*/

class Material
{
public:
	// シェーダーが要求する SRV 1 つ分の情報。m_textureData の並びは
	// m_shaderCache.srvSlots と 1 対 1 で対応する。
	struct TextureData
	{
		std::string SlotName;        // シェーダー内のリソース名 (リフレクション由来)
		UINT        RootParamIndex;  // SetGraphicsRootDescriptorTable に渡すルートパラメータ番号
		std::string TexturePath;     // Texture マネージャのキー。未設定時は DefaultWhiteKey
	};

public:
	/**
	 * @brief Materialの生成
	 * @param materialName Materialの名前
	 * @param vertexPath VertexShaderのパス
	 * @param pixelPath PixelShaderのパス
	 * @param vertexType 頂点レイアウト (PSO のインプットレイアウト決定に使う)
	 * @return 成功したらtrue、失敗したらfalse
	 */
	bool Create(const std::string& materialName, const std::string& vertexPath, const std::string& pixelPath, VertexType vertexType);

	/**
	 * @brief PSO・RootSignature・テクスチャ SRV をコマンドリストへ設定する
	 * @param commandList 記録中のグラフィックスコマンドリスト
	 * @note 定数バッファ (CBV) はマテリアルの管轄外なので設定しない。
	 *       ディスクリプタヒープは呼び出し側が SetDescriptorHeaps 済みであること。
	 */
	void Bind(ID3D12GraphicsCommandList* commandList) const;

	/**
	 * @brief Materialが生成済みかどうかの取得
	 * @return 生成済みならtrue、未生成ならfalse
	 */
	bool IsCreated() const { return m_isCreated; }

	/**
	 * @brief MaterialNameの取得
	 */
	const std::string& GetMaterialName() const { return m_materialName; }

	/**
	 * @brief VertexShaderPathの取得
	 */
	const std::string& GetVertexShaderPath() const { return m_vertexShaderPath; }

	/**
	 * @brief PixelShaderPathの取得
	 */
	const std::string& GetPixelShaderPath() const { return m_pixelShaderPath; }

	/**
	 * @brief 頂点レイアウトの取得
	 */
	VertexType GetVertexType() const { return m_vertexType; }

	/**
	 * @brief PipelineStateの取得 (Shader キャッシュ内の PSO)
	 */
	ID3D12PipelineState* GetPipelineState() const { return m_shaderCache.PSO.Get(); }

	/**
	 * @brief RootSignatureの取得 (Shader キャッシュ内の RootSignature)
	 */
	ID3D12RootSignature* GetRootSignature() const { return m_shaderCache.RootSignature.Get(); }

	/**
	 * @brief RootSignatureのCBV/SRVスロット情報を含むShaderCacheを取得する
	 * @return ShaderCache
	 */
	const ShaderCache& GetShaderCache() const { return m_shaderCache; }

	/**
	 * @brief テクスチャバインディング一覧の取得 (読み取り専用)
	 */
	const std::vector<TextureData>& GetTextureData() const { return m_textureData; }

	/**
	 * @brief 指定スロット (SRV の 0 始まり序数) のテクスチャパスを設定する
	 * @param slotIndex GetTextureData() のインデックスと同じ。範囲外は無視。
	 * @param texturePath 空文字列 / 読み込み失敗時は DefaultWhiteKey にフォールバック
	 */
	void SetTexturePath(UINT slotIndex, const std::string& texturePath);

private:
	/* MaterialName */
	std::string m_materialName;
	/* ShaderPaths */
	std::string m_vertexShaderPath;
	std::string m_pixelShaderPath;
	/* 頂点レイアウト */
	VertexType m_vertexType = VertexType::Default;
	/* Shader シングルトンのキャッシュのコピー (PSO / RootSignature / スロット情報) */
	ShaderCache m_shaderCache;
	/* srvSlots と 1 対 1 のテクスチャバインディング */
	std::vector<TextureData> m_textureData;

	bool m_isCreated = false;
};
