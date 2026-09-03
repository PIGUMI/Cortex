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
private:
	IFloat3 m_Position;
	IFloat3 m_target;
	IFloat3 m_up;
	float m_fovY;
	float m_aspectRationX;
	float m_aspectRationY;
	float m_nearZ;
	float m_farZ;
};

