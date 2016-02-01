#include "dx3.h"
#include <stdexcept>

// source: https://msdn.microsoft.com/en-us/library/windows/desktop/dn859356(v=vs.85).aspx
// [Creating a basic Direct3D 12 components]

#pragma comment(lib, "d3d12")
#pragma comment(lib, "dxgi")
#pragma comment(lib, "d3dcompiler")

struct Vertex
{
	float x, y, z;
	float r, g, b, a;
};

void DirectX12::init(HWND hTargetWnd)
{
	HRESULT hr;

	this->latestFenceValue = 1;

	this->enableDebugLayer();
	hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), &this->pFactory);
	if (FAILED(hr)) throw std::runtime_error("CreateDXGIFactory1");
	ComPtr<IDXGIAdapter1> pAdapter;
	hr = this->pFactory->EnumAdapters1(0, &pAdapter);
	if (FAILED(hr)) throw std::runtime_error("EnumAdapters1");
	hr = D3D12CreateDevice(pAdapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), &this->pDevice);
	if (FAILED(hr)) throw std::runtime_error("D3D12CreateDevice");
	this->createCommandQueue();
	this->createSwapchain(hTargetWnd);
	this->createDescriptorHeapForRTV();
	this->createRenderTarget();
	this->createCommandAllocator();

	// 下の処理は分けたほうがいいかもしれない(けっこう大きい)
	this->initCommandList();
}

void DirectX12::enableDebugLayer()
{
	ComPtr<ID3D12Debug> pDebugLayer;
	HRESULT hr;

	// デバッグインターフェイスの有効化

	hr = D3D12GetDebugInterface(__uuidof(ID3D12Debug), &pDebugLayer);
	if (FAILED(hr)) throw std::runtime_error("D3D12GetDebugInterface");
	pDebugLayer->EnableDebugLayer();
}
void DirectX12::createCommandQueue()
{
	HRESULT hr;
	D3D12_COMMAND_QUEUE_DESC cmdDesc = {};

	cmdDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	cmdDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	hr = this->pDevice->CreateCommandQueue(&cmdDesc, __uuidof(ID3D12CommandQueue), &this->pCommandQueue);
	if (FAILED(hr)) throw std::runtime_error("CreateCommandQueue");
}
void DirectX12::createSwapchain(HWND hTargetWnd)
{
	HRESULT hr;
	DXGI_SWAP_CHAIN_DESC scDesc = {};
	RECT crect;
	ComPtr<IDXGISwapChain> pSwapChain;

	// スワップチェーンの作成(IDXGISwapChain3を使わないといけないあたり以外はだいたいDirectX11と同じ)

	GetClientRect(hTargetWnd, &crect);
	scDesc.BufferCount = 2;
	scDesc.BufferDesc.Width = crect.right - crect.left;
	scDesc.BufferDesc.Height = crect.bottom - crect.top;
	scDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	scDesc.OutputWindow = hTargetWnd;
	scDesc.SampleDesc.Count = 1;
	scDesc.Windowed = true;

	hr = this->pFactory->CreateSwapChain(this->pCommandQueue.Get(), &scDesc, &pSwapChain);
	if (FAILED(hr)) throw std::runtime_error("CreateSwapChain");
	hr = pSwapChain.As(&this->pSwapChain);
	this->throwOnFailed(hr);
	this->frameIndex = this->pSwapChain->GetCurrentBackBufferIndex();
	this->vport.Height = scDesc.BufferDesc.Height;
	this->vport.Width = scDesc.BufferDesc.Width;
	this->vport.MaxDepth = 1.0f;
	this->vport.MinDepth = 0.0f;
	this->vport.TopLeftX = 0;
	this->vport.TopLeftY = 0;
	this->scissor.left = 0;
	this->scissor.top = 0;
	this->scissor.right = this->vport.Width;
	this->scissor.bottom = this->vport.Height;
}
void DirectX12::createDescriptorHeapForRTV()
{
	HRESULT hr;
	D3D12_DESCRIPTOR_HEAP_DESC dhDesc = {};

	// 描画ターゲットビュー用の記述子ヒープを作る

	dhDesc.NumDescriptors = 2;
	dhDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	dhDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	hr = this->pDevice->CreateDescriptorHeap(&dhDesc, __uuidof(ID3D12DescriptorHeap), &this->pDescriptorHeapRTV);
	if (FAILED(hr)) throw std::runtime_error("CreateDescriptorHeap");
}
void DirectX12::createRenderTarget()
{
	HRESULT hr;
	this->hRenderTarget[0] = this->pDescriptorHeapRTV->GetCPUDescriptorHandleForHeapStart();
	this->hRenderTarget[1].ptr = this->hRenderTarget[0].ptr + this->pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	ComPtr<ID3D12Resource> render_texture;

	// 描画ターゲットビューを作る(OMSetRenderTargetsには記述子ハンドルのほう(上)を、
	// ResourceBarrier系APIにはID3D12Resourceのほう(下)を渡す)
	// ダブルバッファリングの制御が必要なので2つとも取得してビューを生成しておく

	for (int i = 0; i < 2; i++)
	{
		hr = this->pSwapChain->GetBuffer(i, __uuidof(ID3D12Resource), &this->pRenderTarget[i]);
		if (FAILED(hr)) throw std::runtime_error("GetBuffer");
		this->pDevice->CreateRenderTargetView(this->pRenderTarget[i].Get(), nullptr, this->hRenderTarget[i]);
	}
}
void DirectX12::createCommandAllocator()
{
	HRESULT hr;

	hr = this->pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), &this->pComAllocator);
	if (FAILED(hr)) throw std::runtime_error("CreateCommandAllocator");
}
void DirectX12::initCommandList()
{
	HRESULT hr;

	// 空のルートシグネチャを作成
	{
		D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
		ComPtr<ID3DBlob> pSignatureBlob, pErrorBlob;
		rsDesc.NumParameters = 0;
		rsDesc.NumStaticSamplers = 0;
		rsDesc.pParameters = nullptr;
		rsDesc.pStaticSamplers = nullptr;
		rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pSignatureBlob, &pErrorBlob);
		this->throwOnFailed(hr);
		hr = this->pDevice->CreateRootSignature(0, pSignatureBlob->GetBufferPointer(), pSignatureBlob->GetBufferSize(),
			__uuidof(ID3D12RootSignature), &this->pRootSignature);
		this->throwOnFailed(hr);
	}

	// シェーダのコンパイルおよび描画用パイプラインステートオブジェクトの作成
	{
		ComPtr<ID3DBlob> pVertexShader, pPixelShader;

		hr = D3DCompileFromFile(L"SimpleVertex.hlsl", nullptr, nullptr, "main", "vs_5_0", 0, 0, &pVertexShader, nullptr);
		this->throwOnFailed(hr);
		hr = D3DCompileFromFile(L"SimplePixel.hlsl", nullptr, nullptr, "main", "ps_5_0", 0, 0, &pPixelShader, nullptr);
		this->throwOnFailed(hr);

		D3D12_INPUT_ELEMENT_DESC ieDesc[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};
		D3D12_GRAPHICS_PIPELINE_STATE_DESC gpsDesc = {};
		gpsDesc.InputLayout = { ieDesc, _countof(ieDesc) };
		gpsDesc.pRootSignature = this->pRootSignature.Get();
		gpsDesc.VS.pShaderBytecode = pVertexShader->GetBufferPointer();
		gpsDesc.VS.BytecodeLength = pVertexShader->GetBufferSize();
		gpsDesc.PS.pShaderBytecode = pPixelShader->GetBufferPointer();
		gpsDesc.PS.BytecodeLength = pPixelShader->GetBufferSize();
		gpsDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		gpsDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
		gpsDesc.RasterizerState.FrontCounterClockwise = false;
		gpsDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
		gpsDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
		gpsDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
		gpsDesc.RasterizerState.DepthClipEnable = true;
		gpsDesc.RasterizerState.MultisampleEnable = false;
		gpsDesc.RasterizerState.AntialiasedLineEnable = false;
		gpsDesc.RasterizerState.ForcedSampleCount = 0;
		gpsDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
		gpsDesc.BlendState.AlphaToCoverageEnable = false;
		gpsDesc.BlendState.IndependentBlendEnable = false;
		gpsDesc.BlendState.RenderTarget[0].BlendEnable = false;
		gpsDesc.BlendState.RenderTarget[0].LogicOpEnable = false;
		gpsDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
		gpsDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
		gpsDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		gpsDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		gpsDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
		gpsDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		gpsDesc.BlendState.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
		gpsDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		gpsDesc.DepthStencilState.DepthEnable = false;
		gpsDesc.DepthStencilState.StencilEnable = false;
		gpsDesc.SampleMask = UINT_MAX;
		gpsDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		gpsDesc.NumRenderTargets = 1;
		gpsDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		gpsDesc.SampleDesc.Count = 1;
		hr = this->pDevice->CreateGraphicsPipelineState(&gpsDesc, __uuidof(ID3D12PipelineState), &this->pStateObject);
		this->throwOnFailed(hr);
	}

	// コマンドリストを作る
	hr = this->pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, this->pComAllocator.Get(), this->pStateObject.Get(), __uuidof(ID3D12CommandList), &this->pCommandList);
	this->throwOnFailed(hr);
	// 初期状態ではコマンドリストはコマンド記録状態なので終了処理をする
	hr = this->pCommandList->Close();
	this->throwOnFailed(hr);

	// 頂点バッファの作成(この辺もDirectX11とだいたい同じ)
	{
		Vertex vts[] = 
		{
			{ 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
			{ 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f },
			{ 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f }
		};
		const auto vbufSize = sizeof(vts);

		D3D12_HEAP_PROPERTIES hprops = {};
		hprops.Type = D3D12_HEAP_TYPE_UPLOAD;
		hprops.CreationNodeMask = 1;
		hprops.VisibleNodeMask = 1;
		D3D12_RESOURCE_DESC rDesc = {};
		rDesc.Width = vbufSize;
		rDesc.Height = 1;
		rDesc.DepthOrArraySize = 1;
		rDesc.MipLevels = 1;
		rDesc.Format = DXGI_FORMAT_UNKNOWN;
		rDesc.SampleDesc.Count = 1;
		rDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		rDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		rDesc.Alignment = 0;
		rDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		hr = this->pDevice->CreateCommittedResource(&hprops, D3D12_HEAP_FLAG_NONE,
			&rDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, __uuidof(ID3D12Resource), &this->pVertexBuffer);
		this->throwOnFailed(hr);
		// 作成と同時に中身を指定する方法はできないっぽい？
		UINT8* pVertexDataBegin;
		D3D12_RANGE readRange;
		readRange.Begin = 0;
		readRange.End = 0;
		hr = this->pVertexBuffer->Map(0, &readRange, (void**)&pVertexDataBegin);
		this->throwOnFailed(hr);
		memcpy(pVertexDataBegin, vts, vbufSize);
		this->pVertexBuffer->Unmap(0, nullptr);

		this->vbufView.BufferLocation = this->pVertexBuffer->GetGPUVirtualAddress();
		this->vbufView.StrideInBytes = sizeof(Vertex);
		this->vbufView.SizeInBytes = vbufSize;
	}

	// 同期用オブジェクト(Fence)の作成
	{
		hr = this->pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), &this->pFence);
		this->throwOnFailed(hr);

		this->hFenceEvent = CreateEvent(nullptr, false, false, L"Fence Event");
		if (this->hFenceEvent == nullptr)
		{
			this->throwOnFailed(HRESULT_FROM_WIN32(GetLastError()));
		}

		this->waitForPreviousFrame();
	}
}
void DirectX12::throwOnFailed(HRESULT hr)
{
	if (FAILED(hr)) throw _com_error(hr);
}

void DirectX12::waitForPreviousFrame()
{
	const UINT64 fence = this->latestFenceValue;
	HRESULT hr = this->pCommandQueue->Signal(this->pFence.Get(), fence);
	this->throwOnFailed(hr);
	this->latestFenceValue++;

	if (this->pFence->GetCompletedValue() < fence)
	{
		hr = this->pFence->SetEventOnCompletion(fence, this->hFenceEvent);
		this->throwOnFailed(hr);
		WaitForSingleObject(this->hFenceEvent, INFINITE);
	}
	this->frameIndex = this->pSwapChain->GetCurrentBackBufferIndex();
}
void DirectX12::updateFrame()
{
	HRESULT hr;

	// コマンド系オブジェクトの初期化
	hr = this->pComAllocator->Reset();
	this->throwOnFailed(hr);
	hr = this->pCommandList->Reset(this->pComAllocator.Get(), this->pStateObject.Get());
	this->throwOnFailed(hr);

	// ルートシグネチャ、ビューポートおよびシザー矩形を設定
	this->pCommandList->SetGraphicsRootSignature(this->pRootSignature.Get());
	this->pCommandList->RSSetViewports(1, &this->vport);
	this->pCommandList->RSSetScissorRects(1, &this->scissor);

	// リソースをレンダリング用にする
	D3D12_RESOURCE_BARRIER barrierToRT = {};
	barrierToRT.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrierToRT.Transition.pResource = this->pRenderTarget[this->frameIndex].Get();
	barrierToRT.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	barrierToRT.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	this->pCommandList->ResourceBarrier(1, &barrierToRT);
	this->pCommandList->OMSetRenderTargets(1, &this->hRenderTarget[this->frameIndex], false, nullptr);

	// ココらへんはDirectX11と同じ(シェーダ周り、IASetInputLayoutがなくなったくらい)
	const float clear_color[] = { 0.0f, 0.0f, 0.0f, 1.0f };
	this->pCommandList->ClearRenderTargetView(this->hRenderTarget[this->frameIndex], clear_color, 0, nullptr);
	this->pCommandList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	this->pCommandList->IASetVertexBuffers(0, 1, &this->vbufView);
	this->pCommandList->DrawInstanced(3, 1, 0, 0);

	// リソースを表示用にする
	D3D12_RESOURCE_BARRIER barrierToPresent = {};
	barrierToPresent.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrierToPresent.Transition.pResource = this->pRenderTarget[this->frameIndex].Get();
	barrierToPresent.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrierToPresent.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	this->pCommandList->ResourceBarrier(1, &barrierToPresent);

	hr = this->pCommandList->Close();
	this->throwOnFailed(hr);

	// CommandListを実行する
	ID3D12CommandList* ppCommands[] = { this->pCommandList.Get() };
	this->pCommandQueue->ExecuteCommandLists(1, ppCommands);
	
	// 表示してGPUの処理待ち
	hr = this->pSwapChain->Present(1, 0);
	this->throwOnFailed(hr);
	this->waitForPreviousFrame();
}
