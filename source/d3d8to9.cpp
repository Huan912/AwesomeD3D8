/**
 * Copyright (C) 2015 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/d3d8to9#license
 */

#include "d3dx9.hpp"
#include "d3d8to9.hpp"
#include "../Minhook/minhook.h"

PFN_D3DXAssembleShader D3DXAssembleShader = nullptr;
PFN_D3DXDisassembleShader D3DXDisassembleShader = nullptr;
PFN_D3DXLoadSurfaceFromSurface D3DXLoadSurfaceFromSurface = nullptr;

#ifndef D3D8TO9NOLOG
 // Very simple logging for the purpose of debugging only.
std::ofstream LOG;
#endif

typedef IDirect3D8 *(WINAPI *Direct3DCreate8_t)(UINT SDKVersion);
Direct3DCreate8_t OrigDirect3DCreate8 = nullptr;
IDirect3D8 *WINAPI MyDirect3DCreate8(UINT SDKVersion)
{
#ifndef D3D8TO9NOLOG
	static bool LogMessageFlag = true;

	if (!LOG.is_open())
	{
		LOG.open("d3d8.log", std::ios::trunc);
	}

	if (!LOG.is_open() && LogMessageFlag)
	{
		LogMessageFlag = false;
		MessageBox(nullptr, TEXT("Failed to open debug log file \"d3d8.log\"!"), nullptr, MB_ICONWARNING);
	}

	LOG << "Redirecting '" << "Direct3DCreate8" << "(" << SDKVersion << ")' ..." << std::endl;
	LOG << "> Passing on to 'Direct3DCreate9':" << std::endl;
#endif

	IDirect3D9 *const d3d = Direct3DCreate9(D3D_SDK_VERSION);

	if (d3d == nullptr)
	{
		return nullptr;
	}

	// Load D3DX
	if (!D3DXAssembleShader || !D3DXDisassembleShader || !D3DXLoadSurfaceFromSurface)
	{
		const HMODULE module = LoadLibrary(TEXT("d3dx9_43.dll"));

		if (module != nullptr)
		{
			D3DXAssembleShader = reinterpret_cast<PFN_D3DXAssembleShader>(GetProcAddress(module, "D3DXAssembleShader"));
			D3DXDisassembleShader = reinterpret_cast<PFN_D3DXDisassembleShader>(GetProcAddress(module, "D3DXDisassembleShader"));
			D3DXLoadSurfaceFromSurface = reinterpret_cast<PFN_D3DXLoadSurfaceFromSurface>(GetProcAddress(module, "D3DXLoadSurfaceFromSurface"));
		}
		else
		{
#ifndef D3D8TO9NOLOG
			LOG << "Failed to load d3dx9_43.dll! Some features will not work correctly." << std::endl;
#endif
			if (MessageBox(nullptr, TEXT(
					"載入d3dx9_43.dll失敗，某些功能將無法正常運作\n\n"
					"若你需要使用該庫，則需要安裝\"Microsoft DirectX End-User Runtime\"，或者從NuGet套件中安裝:\nhttps://www.nuget.org/packages/Microsoft.DXSDK.D3DX\n\n"
					"按下 \"確認\" 將會開啟官方下載點 或 \"取消\" 仍要繼續執行"), nullptr, MB_ICONWARNING | MB_TOPMOST | MB_SETFOREGROUND | MB_OKCANCEL | MB_DEFBUTTON1) == IDOK)
			{
				ShellExecute(nullptr, TEXT("open"), TEXT("https://www.microsoft.com/download/details.aspx?id=35"), nullptr, nullptr, SW_SHOW);

				return nullptr;
			}
		}
	}

	return new Direct3D8(d3d);
}

FARPROC MyGetProcAddress(HMODULE hModule, LPCSTR lpProcName)
{
	if (!hModule)
		return NULL;

	PIMAGE_DOS_HEADER pImageDosHeader;
	PIMAGE_NT_HEADERS pImageNtHeader;
	PIMAGE_EXPORT_DIRECTORY pImageExportDirectory;

	pImageDosHeader = (PIMAGE_DOS_HEADER)hModule;
	pImageNtHeader = (PIMAGE_NT_HEADERS)((DWORD)hModule + pImageDosHeader->e_lfanew);
	pImageExportDirectory = (PIMAGE_EXPORT_DIRECTORY)((DWORD)hModule + pImageNtHeader->OptionalHeader.DataDirectory[0].VirtualAddress);

	DWORD* pAddressOfFunction = (DWORD*)(pImageExportDirectory->AddressOfFunctions + (DWORD)hModule);
	DWORD* pAddressOfNames = (DWORD*)(pImageExportDirectory->AddressOfNames + (DWORD)hModule);
	DWORD  dwNumberOfNames = (DWORD)(pImageExportDirectory->NumberOfNames);
	WORD* pAddressOfNameOrdinals = (WORD*)(pImageExportDirectory->AddressOfNameOrdinals + (DWORD)hModule);
	DWORD dwBase = (DWORD)(pImageExportDirectory->Base);

	DWORD dwName = (DWORD)lpProcName;
	if ((dwName & 0xFFFF0000) == 0)
	{
		if (dwName < dwBase || dwName > dwBase + pImageExportDirectory->NumberOfFunctions - 1)
			return NULL;
		return FARPROC(pAddressOfFunction[dwName - dwBase] + (DWORD)hModule);
	}

	for (int i = 0; i < dwNumberOfNames; i++)
	{
		char* StringFunction = (char*)(pAddressOfNames[i] + (DWORD)hModule);
		if (lstrcmpA(lpProcName, StringFunction) == 0)
		{
			return FARPROC(pAddressOfFunction[pAddressOfNameOrdinals[i]] + (DWORD)hModule);
		}
	}
	return NULL;
}

void HookExport(const char* m_pModule, const char* m_pName, void* m_pHook, void** m_pOriginal)
{
	HMODULE m_hModule = LoadLibraryA(m_pModule);
	if (!m_hModule)
		return;
	void* m_pFunction = reinterpret_cast<void*>(MyGetProcAddress(m_hModule, m_pName)); //Bypass apphelp.dll GetProcAddr Hijack EZPZ
	if (m_pFunction)
	{
		MH_CreateHook(m_pFunction, m_pHook, m_pOriginal);
		MH_STATUS Status = MH_EnableHook(m_pFunction);
		if (Status != MH_OK)
		{
			char szBuffer[256];
			sprintf_s(szBuffer, "發生意外錯誤 Error: 0x%X", Status);
			MessageBoxA(NULL, szBuffer, "Fatal Error", MB_OK | MB_ICONERROR | MB_TOPMOST);
		}
	}
	else
	{
		MessageBoxA(NULL, "發生意外錯誤", "Fatal Error", MB_OK | MB_ICONERROR);
	}
}

/*
void Console()
{
	AllocConsole();
	FILE* sream;
	freopen_s(&sream, ("CON"), ("w"), stdout);
	SetConsoleTitleA(("Console"));
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
}
*/

void InstallHook()
{
	MH_Initialize();
	HookExport("d3d8.dll", "Direct3DCreate8", MyDirect3DCreate8, (void**)&OrigDirect3DCreate8);
}

BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	if (ul_reason_for_call == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(hModule);
		InstallHook();
	}
	return TRUE;
}

