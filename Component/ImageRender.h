#pragma once
#include "Component.h"
#include "ObjectInterface.h"
#include "ShaderLayout.h"

#include <d3d12.h>
#include <wrl/client.h> 

class Camera;
class Material;

/**
 * @brief ImageRenderコンポーネント
 */
class ImageRender : public Component
{
public:// 定義
	enum class ImageRenderType
	{
		UI,
		World,
		Billboard,
	};

	struct ImageParamsCB
	{
		DirectX::XMFLOAT4 color;   // 乗算カラー (w = 不透明度)
	};
public:// メソッド
	ImageRender() = default;
	virtual ~ImageRender() = default;

	ImageRender(const ImageRender&) = delete;
	ImageRender& operator=(const ImageRender&) = delete;

public: // Componentの純粋仮想関数の実装
	void Init() override;
	void Update() override;
	void Render() override;
	void Release() override;
	void Load() override;
	void Save() override;
public: // セッター

	/**
	 * @brief ImageRenderが依存するCameraを設定する
	 * @param camera 依存するCameraのポインタ (借用)
	 */
	void SetCamera(const Camera* camera) { m_pCamera = camera; }

	/**
	 * @brief ImageRenderが依存するMaterialを設定する
	 * @param material 依存するMaterialのポインタ (借用)
	 */
	void SetMaterial(const Material* material) { m_pMaterial = material; }

	/**
	 * @brief ImageRenderの描画タイプを設定する
	 * @param type 描画タイプ
	 */
	void SetImageRenderType(ImageRenderType type) { m_type = type; }

	/**
	 * @brief ImageRenderのTransformを取得する
	 * @return ImageRenderのTransform
	 */
	ITransform& Transform() { return m_transform; }

	/**
	 * @brief ImageRenderのTransformを取得する
	 * @return ImageRenderのTransform
	 */
	const ITransform& Transform() const { return m_transform; }

private:
	// 頂点情報
	Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;
	D3D12_VERTEX_BUFFER_VIEW m_vbv{};
	D3D12_INDEX_BUFFER_VIEW m_ibv{};
	// Transform情報
	ITransform m_transform;
	DirectX::XMFLOAT4 m_color = { 1.0f, 1.0f, 1.0f, 1.0f };
	Microsoft::WRL::ComPtr<ID3D12Resource> m_transformCB;
	void* m_transformMapped = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_paramsCB;
	void* m_paramsMapped = nullptr;
	// 描画タイプ
	ImageRenderType m_type = ImageRenderType::UI;
	// 依存するMaterialとCameraのポインタ (借用)
	const Material* m_pMaterial = nullptr;
	const Camera* m_pCamera = nullptr;
};

