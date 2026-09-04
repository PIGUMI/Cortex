#include "Camera.h"
#include "Window.h"
void Camera::Init()
{
	m_Position = { 0.0f, 0.0f, -5.0f };
	m_target = { 0.0f, 0.0f, 0.0f };
	m_up = { 0.0f, 1.0f, 0.0f };
	m_fovY = DirectX::XMConvertToRadians(60.0f);
	m_aspectRation.x = static_cast<float>(Window::GetInstance()->GetWindowWidth());
	m_aspectRation.y = static_cast<float>(Window::GetInstance()->GetWindowHeight());
	m_nearZ = 0.1f;
	m_farZ = 1000.0f;
}

void Camera::Update()
{
}

void Camera::Release()
{
}

DirectX::XMMATRIX Camera::GetViewMatrix() const
{
	DirectX::XMFLOAT3 pos = m_Position.ToXMFLOAT3();
	DirectX::XMFLOAT3 target = m_target.ToXMFLOAT3();
	DirectX::XMFLOAT3 up = m_up.ToXMFLOAT3();

	return DirectX::XMMatrixLookAtLH(
		DirectX::XMLoadFloat3(&pos),
		DirectX::XMLoadFloat3(&target),
		DirectX::XMLoadFloat3(&up)
	);
}

DirectX::XMMATRIX Camera::GetProjMatrix() const
{
	return DirectX::XMMatrixPerspectiveFovLH(
		m_fovY,
		m_aspectRation.x / m_aspectRation.y,
		m_nearZ,
		m_farZ
	);
}

float Camera::GetDistanceToTarget() const
{
	DirectX::XMFLOAT3 pos = m_Position.ToXMFLOAT3();
	DirectX::XMFLOAT3 target = m_target.ToXMFLOAT3();
	DirectX::XMVECTOR posVec = DirectX::XMLoadFloat3(&pos);
	DirectX::XMVECTOR targetVec = DirectX::XMLoadFloat3(&target);
	DirectX::XMVECTOR diff = DirectX::XMVectorSubtract(posVec, targetVec);
	return DirectX::XMVectorGetX(DirectX::XMVector3Length(diff));
}

void Camera::SetViewMatrix(const DirectX::XMMATRIX& viewMatrix)
{
	using namespace DirectX;

	float distance = GetDistanceToTarget();

	XMMATRIX invView = XMMatrixInverse(nullptr, viewMatrix);

	XMVECTOR up = XMVector3Normalize(invView.r[1]);
	XMVECTOR forward = XMVector3Normalize(invView.r[2]);
	DirectX::XMFLOAT3 target = m_target.ToXMFLOAT3();
	XMVECTOR targetVec = XMLoadFloat3(&target);
	m_target.FromXMFLOAT3(target);
	XMVECTOR eye = XMVectorSubtract(targetVec, XMVectorScale(forward, distance));

	DirectX::XMFLOAT3 eyePos;
	DirectX::XMFLOAT3 upVec;

	XMStoreFloat3(&eyePos, eye);
	XMStoreFloat3(&upVec, up);

	m_Position.FromXMFLOAT3(eyePos);
	m_up.FromXMFLOAT3(upVec);
}
