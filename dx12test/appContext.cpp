#include "appContext.h"
#include <stdexcept>

void AppContext::_init()
{
	WNDCLASSEX wce = { 0 };

	wce.cbSize = sizeof wce;
	wce.hInstance = GetModuleHandle(nullptr);
	wce.lpszClassName = L"dx12test.AppContext";
	wce.lpfnWndProc = &AppContext::WndProc;
	wce.hCursor = LoadCursor(nullptr, IDC_ARROW);
	if (!RegisterClassEx(&wce)) throw std::runtime_error("RegisterClassEx");

	this->hWnd = CreateWindowEx(0, wce.lpszClassName, L"DirectX12 Test", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 640, 480, nullptr, nullptr, wce.hInstance, nullptr);
	if (this->hWnd == nullptr) throw std::runtime_error("CreateWindowEx");
}
int AppContext::run()
{
	this->_init();

	this->pDirectX12 = std::make_unique<DirectX12>();
	this->pDirectX12->init(this->hWnd);

	ShowWindow(this->hWnd, SW_SHOWNORMAL);
	MSG msg;
	while (true)
	{
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE) != 0)
		{
			if (msg.message == WM_QUIT) return msg.wParam;
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		this->pDirectX12->updateFrame();
	}
	return msg.wParam;
}

LRESULT CALLBACK AppContext::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_DESTROY: PostQuitMessage(0); break;
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}
