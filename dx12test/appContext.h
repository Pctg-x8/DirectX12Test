#pragma once

#include <windows.h>
#include <memory>
#include "dx3.h"

class AppContext final
{
	HWND hWnd;
	std::unique_ptr<DirectX12> pDirectX12;

	AppContext() = default;
	void _init();
	static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
public:
	static auto instance()
	{
		static AppContext o;
		return &o;
	}
	
	int run();
};
