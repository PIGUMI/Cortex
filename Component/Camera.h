#pragma once
#include "Component.h"
#include "ObjectInterface.h"

class Camera : public Component
{
public:
	Camera() = default;
	virtual ~Camera() = default;
public:
	void Init() override;
	void Update() override;
	void Release() override;
public:
	/**
	 * @brief View行列を取得する
	 * @return View行列
	 */
	DirectX::XMMATRIX GetViewMatrix() const;

	/**
	 * @brief Projection行列を取得する
	 * @return Projection行列
	 */
	DirectX::XMMATRIX GetProjMatrix() const;

	/**
	 * @brief カメラの位置を取得する
	 * @return カメラの位置
	 */
	IFloat3 GetPosition() const { return m_Position; }

	/**
	 * @brief カメラの位置を設定する
	 * @param position カメラの位置
	 */
	void SetPosition(const IFloat3& position) { m_Position = position; }

	/**
	 * @brief カメラのターゲットを取得する
	 * @param target カメラのターゲット
	 */
	void SetTarget(const IFloat3& target) { m_target = target; }

	/**
	 * @brief カメラのアスペクト比を取得する
	 * @param aspectRatio　カメラのアスペクト比
	 */
	void SetAspectRatio(const IFloat2& aspectRatio) { m_aspectRation = aspectRatio; }

	/**
	 * @brief カメラのターゲットの距離を取得する
	 * @return　カメラのターゲットの距離
	 */
	float GetDistanceToTarget() const;

	/**
	 * @brief View行列からカメラの位置を設定する
	 * @param viewMatrix 
	 */
	void SetViewMatrix(const DirectX::XMMATRIX& viewMatrix);
private:
	IFloat3 m_Position;
	IFloat3 m_target;
	IFloat3 m_up;
	IFloat2 m_aspectRation;
	float m_fovY;
	float m_nearZ;
	float m_farZ;
};

