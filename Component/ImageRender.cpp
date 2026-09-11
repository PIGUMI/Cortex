#include "ImageRender.h"
#include "Material.h"
#include "Shader.h"
#include "Camera.h"
#include "DirectX12.h"

#include <array>
#include <cstdint>
#include <cstring>

void ImageRender::Init()
{
	SetViewName("ImageRender");

	//　頂点設定
	ShaderLayout::Sprite vertices[] =
	{
		{ { -0.5f,  0.5f, 0.0f }, { 0.0f, 0.0f } },
		{ {  0.5f,  0.5f, 0.0f }, { 1.0f, 0.0f } },
		{ { -0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f } },
		{ {  0.5f, -0.5f, 0.0f }, { 1.0f, 1.0f } },
	};
	uint16_t Indices[] = { 0,1,2,0,2,3 };

	UINT CBSize =256;

	// 頂点の作成
	m_vertexBuffer = DirectX::CreateUploadBuffer(sizeof(vertices), vertices);
	if (m_vertexBuffer)
	{
		m_vbv.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
		m_vbv.SizeInBytes = static_cast<UINT> (sizeof(vertices));
		m_vbv.StrideInBytes = sizeof(ShaderLayout::Sprite);
	}
	// インデックス作成
	m_indexBuffer = DirectX::CreateUploadBuffer(sizeof(Indices), Indices);
	if (m_indexBuffer)
	{
		m_ibv.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
		m_ibv.SizeInBytes = static_cast<UINT>(sizeof(Indices));
		m_ibv.Format = DXGI_FORMAT_R16_UINT;
	}

	m_transformCB = DirectX::CreateUploadBuffer(CBSize, nullptr);
	if (m_transformCB)m_transformCB->Map(0, nullptr, &m_transformMapped);

	m_paramsCB = DirectX::CreateUploadBuffer(CBSize, nullptr);
	if (m_paramsCB)m_paramsCB->Map(0, nullptr, &m_paramsMapped);
}

void ImageRender::Update()
{
	using namespace DirectX;

	if (m_transformMapped)
	{
		ShaderLayout::TransfromCB cb{};

		if (m_type == ImageRenderType::UI)
		{
			const float w = static_cast<float>(DirectX::DirectX12::Get()->GetWidth());
			const float h = static_cast<float>(DirectX::DirectX12::Get()->GetHeight());
			cb.world = m_transform.ToXMFLOAT4X4();

			XMStoreFloat4x4(&cb.view, XMMatrixTranspose(XMMatrixIdentity()));
			XMStoreFloat4x4(&cb.projection,XMMatrixTranspose(XMMatrixOrthographicLH(w, h, 0.0f, 1.0f)));
		}
		else if (m_pCamera)
		{
			XMMATRIX world;
			if (m_type == ImageRenderType::Billboard)
			{
				XMMATRIX camRot = XMMatrixTranspose(m_pCamera->GetViewMatrix());
				camRot.r[0] = XMVectorSetW(camRot.r[0], 0.0f);
				camRot.r[1] = XMVectorSetW(camRot.r[1], 0.0f);
				camRot.r[2] = XMVectorSetW(camRot.r[2], 0.0f);
				camRot.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
				const IFloat3 p = m_transform.Position;
				const IFloat3 s = m_transform.Scale;
				world = XMMatrixScaling(s.x, s.y, s.z) * camRot * XMMatrixTranslation(p.x, p.y, p.z);
			}
			else
			{
				world = m_transform.ToMatrix();
			}

			XMStoreFloat4x4(&cb.world,XMMatrixTranspose(world));
			XMStoreFloat4x4(&cb.view, XMMatrixTranspose(m_pCamera->GetViewMatrix()));
			XMStoreFloat4x4(&cb.projection, XMMatrixTranspose(m_pCamera->GetProjMatrix()));
		}
		else
		{
			return;
		}
		std::memcpy(m_transformMapped, &cb, sizeof(cb));
	}

	if (m_paramsMapped)
	{
		const ImageParamsCB params{ m_color };
		std::memcpy(m_paramsMapped, &params, sizeof(params));
	}
}

void ImageRender::Render()
{
	// 例外処理
	if (!m_pMaterial || !m_pMaterial->IsCreated() || !m_vertexBuffer || !m_indexBuffer)return;

	auto* cmd = DirectX::DirectX12::Get()->GetCommandList(DirectX::DirectX12::CmdListType::Direct);
	if (!cmd) return;// 例外処理

	m_pMaterial->Bind(cmd);
	// シェーダーのキャッシュを取得
	const ShaderCache& cache = m_pMaterial->GetShaderCache();

	for (const auto& cbv : cache.cbvSlots)
	{
		if (cbv.name == "Transform" && m_transformCB)
		{
			cmd->SetGraphicsRootConstantBufferView(cbv.index, m_transformCB->GetGPUVirtualAddress());
		}else if(cbv.name == "ImageParams" && m_paramsCB)
		{
			cmd->SetGraphicsRootConstantBufferView(cbv.index, m_paramsCB->GetGPUVirtualAddress());
		}

		cmd->IASetVertexBuffers(0, 1, &m_vbv);
		cmd->IASetIndexBuffer(&m_ibv);
		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		cmd->DrawIndexedInstanced(static_cast<UINT>(6), 1, 0, 0, 0);
	}
}

void ImageRender::Release()
{
	// トランスフォームの初期化
	if (m_transformCB)
	{
		m_transformCB->Unmap(0, nullptr);
		m_transformMapped = nullptr;
		m_transformCB.Reset();
	}
	// パラメータの初期化
	if(m_paramsCB)
	{
		m_paramsCB->Unmap(0, nullptr);
		m_paramsMapped = nullptr;
		m_paramsCB.Reset();
	}
	m_vertexBuffer.Reset();
	m_indexBuffer.Reset();
	m_vbv = {};
	m_ibv = {};

	m_pMaterial = nullptr;
	m_pCamera = nullptr;
}

void ImageRender::Load()
{
	/*
	* ToDo
	* 今後実装予定
	*/
}

void ImageRender::Save()
{
	/*
	* ToDo
	* 今後実装予定
	*/
}
