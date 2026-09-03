#pragma once
#include <DirectXMath.h>

struct IFloat2
{
	float x = 0;
	float y = 0;

	/**
	 * @brief IFloat2をDirectX::XMFLOAT2に変換する
	 * @return 変換後のDirectX::XMFLOAT2
	 */
	DirectX::XMFLOAT2 ToXMFLOAT2() const { return DirectX::XMFLOAT2(x, y); }

	/**
	 * @brief DirectX::XMFLOAT2からIFloat2に変換する
	 * @param float2 変換元のDirectX::XMFLOAT2
	 */
	void FromXMFLOAT2(const DirectX::XMFLOAT2& float2)
	{
		x = float2.x;
		y = float2.y;
	}
};

struct IFloat3
{
	float x = 0;
	float y = 0;
	float z = 0;

	/**
	 * @brief IFloat3をDirectX::XMFLOAT3に変換する
	 * @return 変換後のDirectX::XMFLOAT3
	 */
	DirectX::XMFLOAT3 ToXMFLOAT3() const { return DirectX::XMFLOAT3(x, y, z); }

	/**
	 * @brief DirectX::XMFLOAT3からIFloat3に変換する
	 * @param float3 変換元のDirectX::XMFLOAT3
	 */
	void FromXMFLOAT3(const DirectX::XMFLOAT3& float3)
	{
		x = float3.x;
		y = float3.y;
		z = float3.z;
	}
};

struct IFloat4
{
	float x = 0;
	float y = 0;
	float z = 0;
	float w = 0;
	/**
	 * @brief IFloat4をDirectX::XMFLOAT4に変換する
	 * @return 変換後のDirectX::XMFLOAT4
	 */
	DirectX::XMFLOAT4 ToXMFLOAT4() const { return DirectX::XMFLOAT4(x, y, z, w); }

	/**
	 * @brief DirectX::XMFLOAT4からIFloat4に変換する
	 * @param float4 変換元のDirectX::XMFLOAT4
	 */
	void FromXMFLOAT4(const DirectX::XMFLOAT4& float4)
	{
		x = float4.x;
		y = float4.y;
		z = float4.z;
		w = float4.w;
	}
};

struct ITransform
{
	IFloat3 Position;
	IFloat3 Rotation;
	IFloat3 Scale;

	/**
	 * @brief Transformを行列に変換する
	 * @return 変換後の行列
	 */
	DirectX::XMMATRIX ToMatrix() const
	{
		using namespace DirectX;
		XMMATRIX translation = XMMatrixTranslation(Position.x, Position.y, Position.z);
		XMMATRIX rotation = XMMatrixRotationRollPitchYaw(DirectX::XMConvertToRadians(Rotation.x), DirectX::XMConvertToRadians(Rotation.y), DirectX::XMConvertToRadians(Rotation.z));
		XMMATRIX scale = XMMatrixScaling(Scale.x, Scale.y, Scale.z);
		return scale * rotation * translation;
	}

	/**
	 * @brief 行列からTransformに変換する
	 * @param matrix 変換元の行列
	 */
	void FromMatrix(const DirectX::XMMATRIX& matrix)
	{
		using namespace DirectX;
		XMFLOAT4X4 float4x4;
		XMStoreFloat4x4(&float4x4, matrix);
		Position.x = float4x4._41;
		Position.y = float4x4._42;
		Position.z = float4x4._43;
		// Extract rotation angles from the matrix
		float sy = (float)sqrt(float4x4._11 * float4x4._11 + float4x4._21 * float4x4._21);
		bool singular = sy < 1e-6;
		if (!singular)
		{
			Rotation.x = (float)atan2(float4x4._32, float4x4._33);
			Rotation.y = (float)atan2(-float4x4._31, sy);
			Rotation.z = (float)atan2(float4x4._21, float4x4._11);
		}
		else
		{
			Rotation.x = (float)atan2(-float4x4._23, float4x4._22);
			Rotation.y = (float)atan2(-float4x4._31, sy);
			Rotation.z = 0;
		}
		Rotation.x = DirectX::XMConvertToDegrees(Rotation.x);
		Rotation.y = DirectX::XMConvertToDegrees(Rotation.y);
		Rotation.z = DirectX::XMConvertToDegrees(Rotation.z);
		Scale.x = (float)sqrt(float4x4._11 * float4x4._11 + float4x4._21 * float4x4._21 + float4x4._31 * float4x4._31);
		Scale.y = (float)sqrt(float4x4._12 * float4x4._12 + float4x4._22 * float4x4._22 + float4x4._32 * float4x4._32);
		Scale.z = (float)sqrt(float4x4._13 * float4x4._13 + float4x4._23 * float4x4._23 + float4x4._33 * float4x4._33);
	}

	/**
	 * @brief Transformをfloat配列に変換する
	 * @param outArray 変換後のfloat配列（16要素）
	 */
	void ToFloats(float* outArray) const
	{
		DirectX::XMMATRIX matrix = ToMatrix();
		DirectX::XMStoreFloat4x4(reinterpret_cast<DirectX::XMFLOAT4X4*>(outArray), matrix);
	}

	/**
	 * @brief float配列からTransformに変換する
	 * @param inArray 変換元のfloat配列（16要素）
	 */
	void FromFloats(const float* inArray)
	{
		DirectX::XMMATRIX matrix = DirectX::XMLoadFloat4x4(reinterpret_cast<const DirectX::XMFLOAT4X4*>(inArray));
		FromMatrix(matrix);
	}

	/**
	 * @brief TransformをDirectX::XMFLOAT4X4に変換する
	 * @return 変換後のDirectX::XMFLOAT4X4
	 */
	DirectX::XMFLOAT4X4 ToXMFLOAT4X4() const
	{
		DirectX::XMMATRIX matrix = ToMatrix();
		DirectX::XMFLOAT4X4 float4x4;
		DirectX::XMStoreFloat4x4(&float4x4, DirectX::XMMatrixTranspose(matrix));
		return float4x4;
	}
};
