#include "machine.hpp"
#include <cstdlib>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dxgi.h>
#endif

namespace tuner {
std::string vendor_name(unsigned vendor_id) {
    switch (vendor_id) {
        case 0x10DE: return "nvidia";
        case 0x1002: case 0x1022: return "amd";
        case 0x8086: return "intel";
        default: return "";
    }
}

Machine detect_machine() {
    Machine m;
#ifdef _WIN32
    char buffer[256];
    if (GetEnvironmentVariableA("SteamDeck", buffer, sizeof buffer) > 0 && buffer[0] == '1') m.steam_deck = true;
    if (auto* ntdll = GetModuleHandleA("ntdll.dll")) {
        using Fn = const char* (CDECL*)();
        if (auto fn = reinterpret_cast<Fn>(GetProcAddress(ntdll, "wine_get_version"))) { if (const char* v = fn()) { m.wine = true; m.wine_version = v; } }
    }
    // The adapter: DXGI is already loaded by the game (D3D12), so this is a lookup, not a
    // driver start. Under Proton the DXGI comes from DXVK and reports the real vendor and
    // memory. The adapter with the most dedicated memory is the one the game draws with
    // (a software adapter has none).
    if (HMODULE dxgi = GetModuleHandleA("dxgi.dll")) {
        using CreateFn = HRESULT (WINAPI*)(REFIID, void**);
        if (auto create = reinterpret_cast<CreateFn>(GetProcAddress(dxgi, "CreateDXGIFactory1"))) {
            IDXGIFactory1* factory = nullptr;
            if (SUCCEEDED(create(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory))) && factory) {
                IDXGIAdapter1* adapter = nullptr;
                for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
                    DXGI_ADAPTER_DESC1 desc{};
                    if (SUCCEEDED(adapter->GetDesc1(&desc)) && !(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
                        const double mb = double(desc.DedicatedVideoMemory) / (1024.0 * 1024.0);
                        if (mb > m.vram_mb) {
                            m.vram_mb = mb;
                            m.gpu_vendor = vendor_name(desc.VendorId);
                            char name[256]; const int n = WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, name, sizeof name, nullptr, nullptr);
                            m.gpu_name = n > 0 ? std::string(name) : std::string();
                            // Under Proton, DXVK can present an NVIDIA card with AMD's vendor id (dxgi.hideNvidiaGpu)
                            // while keeping its name; the name is the truth then.
                            if (m.gpu_name.find("NVIDIA") != std::string::npos || m.gpu_name.find("GeForce") != std::string::npos) m.gpu_vendor = "nvidia";
                            else if (m.gpu_name.find("Radeon") != std::string::npos) m.gpu_vendor = "amd";
                            else if (m.gpu_name.find("Intel") != std::string::npos || m.gpu_name.find("Arc") != std::string::npos) m.gpu_vendor = "intel";
                        }
                    }
                    adapter->Release();
                }
                factory->Release();
            }
        }
    }
#endif
    return m;
}
}
