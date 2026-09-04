#pragma once
#include "Component.h"
#include "ObjectInterface.h"   // ITransform / IFloat3
#include "ShaderLayout.h"      // ShaderLayout::LightingParamsCB / NodeGraphParamsCB
#include <d3d12.h>
#include <wrl/client.h>

class Model;
class Camera;

/*
* MeshRender
* Model (アセット) を借用し、インスタンス側の状態 (transform / 定数バッファ) を持って描画するコンポーネント。
* Model は所有しない。使う前に SetModel() と SetCamera() を呼ぶこと。
*/
class MeshRender : public Component
{
public:
	MeshRender() = default;
	~MeshRender() override;

	MeshRender(const MeshRender&) = delete;
	MeshRender& operator=(const MeshRender&) = delete;

	/**
	 * @brief 描画対象の Model を設定し、サブメッシュ数ぶんの定数バッファを確保する
	 * @param model 借用ポインタ (nullptr で解除)
	 */
	void SetModel(const Model* model);

	/**
	 * @brief view / projection / カメラ座標の取得元カメラを設定する (借用)
	 */
	void SetCamera(const Camera* camera) { m_camera = camera; }

	// --- Component ---
	void Init() override;      // 既定値の初期化 (定数バッファ確保は SetModel 側)
	void Update() override;    // transform / lighting 定数バッファを更新
	void Render() override;    // サブメッシュを描画
	void Release() override;   // 定数バッファを解放
	void Load() override {}    // TODO: シリアライズ対応
	void Save() override {}    // TODO: シリアライズ対応

	ITransform& Transform() { return m_transform; }
	const ITransform& Transform() const { return m_transform; }

	/**
	 * @brief 指定サブメッシュの tint (ノードグラフ評価結果) を設定する
	 * @param subMeshIndex 対象サブメッシュのインデックス (範囲外は無視)
	 */
	void SetTint(int subMeshIndex, const ShaderLayout::NodeGraphParamsCB& tint);

	/**
	 * @brief ディレクショナルライトの向きを設定する (正規化不要)
	 */
	void SetLightDirection(const IFloat3& dir);

private:
	void ReleaseConstantBuffers();

private:
	const Model*  m_model  = nullptr;   // 借用。所有しない
	const Camera* m_camera = nullptr;   // 借用。所有しない
	// ITransform の Scale 既定は {0,0,0} なので {1,1,1} で初期化しておく
	ITransform    m_transform{ IFloat3{}, IFloat3{}, IFloat3{ 1.0f, 1.0f, 1.0f } };

	// transform 用 (全サブメッシュ共通、1 つ)
	Microsoft::WRL::ComPtr<ID3D12Resource> m_transformCB;
	void* m_transformMapped = nullptr;

	// tint 用 (サブメッシュごとに 256B アライメントで並べる)
	Microsoft::WRL::ComPtr<ID3D12Resource> m_tintCB;
	void* m_tintMapped = nullptr;
	int   m_subMeshCount = 0;
	static constexpr UINT kTintStride = 256;

	// スペキュラ・リムライト用 (全サブメッシュ共通、1 つ)
	Microsoft::WRL::ComPtr<ID3D12Resource> m_lightingCB;
	void* m_lightingMapped = nullptr;
	ShaderLayout::LightingParamsCB m_lightingParams;
};
