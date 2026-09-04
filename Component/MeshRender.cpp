#include "MeshRender.h"
#include "Model.h"        // Model / SubMesh (+ 経由で ShaderCache)
#include "Shader.h"       // ShaderCache / SlotInfo
#include "Camera.h"
#include "DirectX12.h"    // DirectX::DirectX12 / DirectX::CreateUploadBuffer
#include <cstdint>
#include <cstring>

MeshRender::~MeshRender()
{
	ReleaseConstantBuffers();
}

void MeshRender::Init()
{
	SetViewName("MeshRender");
	m_transform.Position = { 0.0f, 0.0f, 0.0f };
	m_transform.Rotation = { 0.0f, 0.0f, 0.0f };
	m_transform.Scale    = { 1.0f, 1.0f, 1.0f };
}

void MeshRender::SetModel(const Model* model)
{
	ReleaseConstantBuffers();
	m_model = model;
	if (!model)
	{
		return;
	}

	m_subMeshCount = static_cast<int>(model->GetSubMeshes().size());

	// transform (256B アライメント)
	m_transformCB = DirectX::CreateUploadBuffer(256, nullptr);
	if (m_transformCB)
	{
		m_transformCB->Map(0, nullptr, &m_transformMapped);
	}

	// tint (サブメッシュごとに 256B ずつ並べる)
	if (m_subMeshCount > 0)
	{
		m_tintCB = DirectX::CreateUploadBuffer(static_cast<size_t>(kTintStride) * m_subMeshCount, nullptr);
		if (m_tintCB)
		{
			m_tintCB->Map(0, nullptr, &m_tintMapped);

			// 未初期化の Upload ヒープのままだと tintMode 等が不定値になるため既定値で埋める
			const ShaderLayout::NodeGraphParamsCB defaultTint{};
			for (int i = 0; i < m_subMeshCount; ++i)
			{
				std::memcpy(static_cast<uint8_t*>(m_tintMapped) + static_cast<size_t>(i) * kTintStride,
				            &defaultTint, sizeof(defaultTint));
			}
		}
	}

	// lighting (256B アライメント)
	m_lightingCB = DirectX::CreateUploadBuffer(256, nullptr);
	if (m_lightingCB)
	{
		m_lightingCB->Map(0, nullptr, &m_lightingMapped);
		std::memcpy(m_lightingMapped, &m_lightingParams, sizeof(m_lightingParams));
	}
}

void MeshRender::Update()
{
	if (!m_camera)
	{
		return;
	}

	if (m_transformMapped)
	{
		ShaderLayout::TransfromCB cb{};
		cb.world = m_transform.ToXMFLOAT4X4(); // ToXMFLOAT4X4 は転置済み (HLSL 用)
		DirectX::XMStoreFloat4x4(&cb.view,       DirectX::XMMatrixTranspose(m_camera->GetViewMatrix()));
		DirectX::XMStoreFloat4x4(&cb.projection, DirectX::XMMatrixTranspose(m_camera->GetProjMatrix()));
		std::memcpy(m_transformMapped, &cb, sizeof(cb));
	}

	if (m_lightingMapped)
	{
		// lightDir は SetLightDirection 側で更新済み。ここでは cameraPos だけ更新してまとめて書く
		m_lightingParams.cameraPos = m_camera->GetPosition().ToXMFLOAT3();
		std::memcpy(m_lightingMapped, &m_lightingParams, sizeof(m_lightingParams));
	}
}

void MeshRender::Render()
{
	if (!m_model || !m_model->IsLoaded())
	{
		return;
	}

	auto* cmd = DirectX::DirectX12::Get()->GetCommandList(DirectX::DirectX12::CmdListType::Direct);
	if (!cmd)
	{
		return;
	}

	const std::vector<SubMesh>& subMeshes = m_model->GetSubMeshes();
	for (size_t i = 0; i < subMeshes.size(); ++i)
	{
		const SubMesh& sm = subMeshes[i];
		if (!sm.isActive)
		{
			continue;
		}

		// マテリアル所有物: PSO / RootSignature / テクスチャ SRV ディスクリプタテーブル
		sm.material.Bind(cmd);

		// インスタンス側の CBV は名前でルートパラメータ番号を引いて設定する
		// (シェーダーリフレクションが返す並び順に依存しないようにするため)
		const ShaderCache& cache = sm.material.GetShaderCache();
		for (const auto& cbv : cache.cbvSlots)
		{
			if (cbv.name == "Transform" && m_transformCB)
			{
				cmd->SetGraphicsRootConstantBufferView(cbv.index, m_transformCB->GetGPUVirtualAddress());
			}
			else if (cbv.name == "NodeGraphParams" && m_tintCB)
			{
				const UINT64 offset = static_cast<UINT64>(i) * kTintStride;
				cmd->SetGraphicsRootConstantBufferView(cbv.index, m_tintCB->GetGPUVirtualAddress() + offset);
			}
			else if (cbv.name == "LightingParams" && m_lightingCB)
			{
				cmd->SetGraphicsRootConstantBufferView(cbv.index, m_lightingCB->GetGPUVirtualAddress());
			}
		}

		cmd->IASetVertexBuffers(0, 1, &sm.vertexBufferView);
		cmd->IASetIndexBuffer(&sm.indexBufferView);
		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmd->DrawIndexedInstanced(sm.IndexCount, 1, 0, 0, 0);
	}
}

void MeshRender::Release()
{
	ReleaseConstantBuffers();
	m_model  = nullptr;
	m_camera = nullptr;
}

void MeshRender::SetTint(int subMeshIndex, const ShaderLayout::NodeGraphParamsCB& tint)
{
	if (!m_tintMapped || subMeshIndex < 0 || subMeshIndex >= m_subMeshCount)
	{
		return;
	}
	std::memcpy(static_cast<uint8_t*>(m_tintMapped) + static_cast<size_t>(subMeshIndex) * kTintStride,
	            &tint, sizeof(tint));
}

void MeshRender::SetLightDirection(const IFloat3& dir)
{
	m_lightingParams.lightDir = dir.ToXMFLOAT3();
	if (m_lightingMapped)
	{
		std::memcpy(m_lightingMapped, &m_lightingParams, sizeof(m_lightingParams));
	}
}

void MeshRender::ReleaseConstantBuffers()
{
	if (m_transformCB)
	{
		m_transformCB->Unmap(0, nullptr);
		m_transformMapped = nullptr;
		m_transformCB.Reset();
	}
	if (m_tintCB)
	{
		m_tintCB->Unmap(0, nullptr);
		m_tintMapped = nullptr;
		m_tintCB.Reset();
	}
	if (m_lightingCB)
	{
		m_lightingCB->Unmap(0, nullptr);
		m_lightingMapped = nullptr;
		m_lightingCB.Reset();
	}
	m_subMeshCount = 0;
}
