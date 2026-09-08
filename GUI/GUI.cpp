#include "GUI.h"

#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"

#include "DirectX12.h"   // DirectX::DirectX12 / DirectX::Descriptor

GUI* GUI::s_instance = nullptr;

namespace
{
	// ImGui のフォントアトラス等の SRV を、Cortex の Descriptor シングルトンのヒープから貸す。
	// (1.92 以降、動的フォントのためこのコールバックは複数回呼ばれる)
	void SrvDescriptorAlloc(ImGui_ImplDX12_InitInfo*,
	                        D3D12_CPU_DESCRIPTOR_HANDLE* outCpu,
	                        D3D12_GPU_DESCRIPTOR_HANDLE* outGpu)
	{
		const UINT index = DirectX::Descriptor::Get()->Allocate();
		*outCpu = DirectX::Descriptor::Get()->GetCPUHandle(index);
		*outGpu = DirectX::Descriptor::Get()->GetGPUHandle(index);
	}

	void SrvDescriptorFree(ImGui_ImplDX12_InitInfo*,
	                       D3D12_CPU_DESCRIPTOR_HANDLE cpu,
	                       D3D12_GPU_DESCRIPTOR_HANDLE)
	{
		DirectX::Descriptor* desc = DirectX::Descriptor::Get();
		const SIZE_T heapStart = desc->GetHeap()->GetCPUDescriptorHandleForHeapStart().ptr;
		const UINT index = static_cast<UINT>((cpu.ptr - heapStart) / desc->GetDescriptorSize());
		desc->Free(index);
	}
}

GUI* GUI::Get()
{
	if (s_instance == nullptr)
	{
		s_instance = new GUI();
	}
	return s_instance;
}

void GUI::Del()
{
	if (s_instance != nullptr)
	{
		s_instance->Shutdown();
		delete s_instance;
		s_instance = nullptr;
	}
}

bool GUI::Init(HWND hwnd)
{
	if (m_initialized)
	{
		return true;
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	// multi-viewport (ImGui ウィンドウをメインウィンドウの外へ独立 OS ウィンドウ化) は
	// プラットフォーム統合を詰めるまで無効。docking はメインウィンドウ内で機能する。
	// io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	m_viewportsEnabled = (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0;

	ImGui::StyleColorsDark();
	if (m_viewportsEnabled)
	{
		// マルチビューポート時はメインとサブウィンドウの見た目を揃える
		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	if (!ImGui_ImplWin32_Init(hwnd))
	{
		ImGui::DestroyContext();
		return false;
	}

	DirectX::DirectX12* dx = DirectX::DirectX12::Get();

	ImGui_ImplDX12_InitInfo info = {};
	info.Device               = dx->GetDevice();
	info.CommandQueue         = dx->GetDirectCommandQueue();   // マルチビューポートのテクスチャ転送に使う
	info.NumFramesInFlight    = 2;                             // スワップチェーンのバックバッファ数
	info.RTVFormat            = DXGI_FORMAT_R8G8B8A8_UNORM;
	info.DSVFormat            = DXGI_FORMAT_UNKNOWN;           // ImGui は深度を使わない
	info.SrvDescriptorHeap    = DirectX::Descriptor::Get()->GetHeap();
	info.SrvDescriptorAllocFn = SrvDescriptorAlloc;
	info.SrvDescriptorFreeFn  = SrvDescriptorFree;

	if (!ImGui_ImplDX12_Init(&info))
	{
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		return false;
	}

	m_initialized = true;
	return true;
}

void GUI::Shutdown()
{
	if (!m_initialized)
	{
		return;
	}
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	m_initialized = false;
}

void GUI::BeginFrame()
{
	if (!m_initialized)
	{
		return;
	}
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void GUI::EndFrame(ID3D12GraphicsCommandList* commandList)
{
	if (!m_initialized)
	{
		return;
	}
	ImGui::Render();
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
}

void GUI::RenderMultiViewport()
{
	if (!m_initialized || !m_viewportsEnabled)
	{
		return;
	}
	ImGui::UpdatePlatformWindows();
	ImGui::RenderPlatformWindowsDefault();
}
