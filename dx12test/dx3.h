#pragma once

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl.h>
#include <comdef.h>
#include <d3dcompiler.h>

class DirectX12 final
{
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	ComPtr<IDXGIFactory1> pFactory;
	ComPtr<ID3D12Device> pDevice;
	ComPtr<ID3D12CommandQueue> pCommandQueue;
	ComPtr<IDXGISwapChain3> pSwapChain;
	ComPtr<ID3D12DescriptorHeap> pDescriptorHeapRTV;
	ComPtr<ID3D12Resource> pRenderTarget[2];
	D3D12_CPU_DESCRIPTOR_HANDLE hRenderTarget[2];
	ComPtr<ID3D12CommandAllocator> pComAllocator;
	ComPtr<ID3D12RootSignature> pRootSignature;
	ComPtr<ID3D12PipelineState> pStateObject;
	ComPtr<ID3D12GraphicsCommandList> pCommandList;
	ComPtr<ID3D12Resource> pVertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW vbufView;
	ComPtr<ID3D12Fence> pFence;
	HANDLE hFenceEvent;
	UINT64 latestFenceValue;
	UINT frameIndex;
	D3D12_VIEWPORT vport;
	D3D12_RECT scissor;

	void enableDebugLayer();
	void createCommandQueue();
	void createSwapchain(HWND hTargetWnd);
	void createDescriptorHeapForRTV();
	void createRenderTarget();
	void createCommandAllocator();
	void initCommandList();
	void throwOnFailed(HRESULT hr);
public:
	void init(HWND hTargetWnd);

	void updateFrame();
	void waitForPreviousFrame();
};
